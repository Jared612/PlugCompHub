/**
 * @file pcxcore.cpp
 * @brief 内置核心组件装配：实现 `pcxcoreStart` / `pcxcoreStop`。
 * @details 读内置组件表、创建核心对象并注册到 `ObjectManager` 与 `ComponentManager`。
 */

#include <assert.h>
#include <string.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "pcxcomponent.h"
#include "pcxplugin.h"
#include "componentManager.h"
#include "messagecenter.h"
#include "objectManager.h"
#include "pluginManager.h"
#include "loggerManager.h"
#include "environment.h"

/**
 * @brief 内置组件导出表
 * @details
 * 内置组件导出表，用于在 PCX 核心运行时中注册组件。
 * 该表在启动阶段由 `PCX_GET_COMPONENT_TABLE` 读取并参与核心对象装配。
 */	
PCX_COMPONENT_EXPORT_TABLE_BEGIN(_cmptable)             // 定义内置组件导出表 _cmptable
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::PluginManager)     // 注册插件管理器组件
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::ComponentManager)  // 注册组件管理器组件
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::ObjectManager)     // 注册对象管理器组件
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::MessageCenter)     // 注册消息中心组件
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::LoggerManager)     // 注册日志管理器组件
PCX_COMPONENT_EXPORT_TABLE_ITEM(pcx::Environment)       // 注册环境管理器组件
PCX_COMPONENT_EXPORT_TABLE_END()                        // 结束内置组件导出表

PCX_BEGIN_NAMESPACE


extern ObjectManager* _objManager;			/** 声明（非定义）：`_objManager` 在 `pcx.cpp` 中定义并分配存储，此处仅引用同一全局变量。 */
extern ComponentManager* _componentManager;	/** 同上，`extern` 表示链接到 `pcx.cpp` 中的唯一定义。 */

/**
 * @brief 实现 `pcxcoreStart`，启动 PCX 核心运行时。
 * @return 启动成功返回 ObjectManager 接口，失败返回 nullptr
 */
__PCX_API IObjectManager* pcxcoreStart()
{
	ComponentInfo** cmptable = PCX_GET_COMPONENT_TABLE(_cmptable);
	if (cmptable == nullptr) {
		WriteLog(PcxLogLevel::Error, "The embedded table have troubles");
		return nullptr;
	}

	// 核心对象默认实例名映射表
	std::unordered_map<std::string, std::string> comIDName = {
		{PCX_COMPONENT_MANAGER_ID, PCX_DEFAULT_COMPONENTMANAGER},
		{PCX_OBJECT_MANAGER_ID,    PCX_DEFAULT_OBJECTMANAGER},
		{PCX_PLUGIN_MANAGER_ID,    PCX_DEFAULT_PLUGINMANAGER},
		{PCX_MESSAGECENTER_ID,     PCX_DEFAULT_MESSAGECENTER},
		{PCX_LOGGERMANAGER_ID,     PCX_DEFAULT_LOGGERMANAGER},
		{PCX_ENVIROMENT_ID,        PCX_DEFAULT_ENVIROMENT}
	};

	// 记录 (ComponentInfo*, Component*) 的一一配对关系；用显式 pair 容器替代按 i 下标共享的两个数组，
	// 避免遇到 continue/跳过项时 cmptable[i] 与 tempVec[i] 索引错位。
	struct Entry
	{
		ComponentInfo* info;
		Component*     component;
		ObjectInfo*    objInfo;
	};
	std::vector<Entry> entries;
	std::unordered_map<std::string, ObjectInfo*> objInfoMap;

	for (int i = 0; cmptable[i] != nullptr; i++) {
		ComponentInfo* ci = cmptable[i];
		if (ci->componentID == nullptr || ci->componentID[0] == '\0') {
			WriteLog(PcxLogLevel::Error, "componentID of cmptable[%d] is nullptr", i);
			continue;
		}
		if (ci->creator == nullptr || ci->deletor == nullptr) {
			WriteLog(PcxLogLevel::Error, "Creator or deletor of component[%s] is nullptr", ci->componentID);
			continue;
		}

		Component* component = new (std::nothrow) Component(ci);
		if (component == nullptr) {
			WriteLog(PcxLogLevel::Fatal, "alloc Component for [%s] failed", ci->componentID);
			continue;
		}
		ObjectInfo* objInfo = createObjectInfo(component, nullptr);
		if (objInfo == nullptr) {
			WriteLog(PcxLogLevel::Fatal, "createObjectInfo for [%s] failed", ci->componentID);
			delete component;
			continue;
		}

		if (strcmp(ci->componentID, PCX_OBJECT_MANAGER_ID) == 0) {
			_objManager = static_cast<ObjectManager*>(objInfo->object);
		}
		objInfoMap[ci->componentID] = objInfo;
		entries.push_back({ ci, component, objInfo });
	}

	if (_objManager == nullptr) {
		WriteLog(PcxLogLevel::Fatal, "ObjectManager is not built during pcxcoreStart");
		// 回滚已创建的对象以免泄漏（此时尚未注册到 ObjectManager，直接用 ComponentInfo::deletor 释放）
		for (auto& e : entries) {
			if (e.objInfo) {
				if (e.objInfo->object && e.info && e.info->deletor) {
					try { e.info->deletor(e.objInfo->object); } catch (...) {}
				}
				delete e.objInfo;
			}
			delete e.component;
		}
		return nullptr;
	}

	// 注册核心对象默认实例名
	for (auto& kv : objInfoMap) {
		auto nameIt = comIDName.find(kv.first);
		if (nameIt == comIDName.end()) {
			continue;
		}
		_objManager->registerObj(kv.second, nameIt->second.c_str());
	}

	// 获取组件管理器对象
	auto cmIt = objInfoMap.find(PCX_COMPONENT_MANAGER_ID);
	if (cmIt == objInfoMap.end() || cmIt->second == nullptr) {
		WriteLog(PcxLogLevel::Fatal, "ComponentManager is missing");
		return nullptr;
	}
	_componentManager = static_cast<ComponentManager*>(cmIt->second->object);
	if (_componentManager == nullptr) {
		WriteLog(PcxLogLevel::Fatal, "ComponentManager object is nullptr");
		return nullptr;
	}

	// 将组件信息与对应的 Component* 封装注册进 ComponentManager（不再依赖并行索引）
	for (auto& e : entries) {
		_componentManager->registerComponent(e.info, e.component);
	}

	return _objManager;
}

/**
 * @brief 实现 `pcxcoreStop`，停止 PCX 核心运行时并释放核心管理对象。
 */
__PCX_API void pcxcoreStop()
{
	if (_objManager == nullptr) {
		_componentManager = nullptr;
		return;
	}

	// 1) 先销毁业务对象与叶子管理器（LoggerManager/MessageCenter/Environment），
	//    同时把 PluginManager/ComponentManager/ObjectManager 的 ObjectInfo 保留
	_objManager->tearDown();

	// 2) 再按"先 PluginManager（卸载 DLL 会使插件 ComponentInfo* 失效）、再 ComponentManager、
	//    最后 ObjectManager"的顺序销毁核心管理器自身。
	ObjectInfo* pluginInfo = _objManager->getObjInfo(PCX_DEFAULT_PLUGINMANAGER);
	ObjectInfo* compInfo = _objManager->getObjInfo(PCX_DEFAULT_COMPONENTMANAGER);
	ObjectInfo* objMgrInfo = _objManager->getObjInfo(PCX_DEFAULT_OBJECTMANAGER);

	auto destroyInfo = [](ObjectInfo* info) {
		if (info == nullptr) {
			return;
		}
		if (info->object && info->component
			&& info->component->getComponentInfo()
			&& info->component->getComponentInfo()->deletor) {
			try {
				info->component->getComponentInfo()->deletor(info->object);
			} catch (...) {
			}
		}
		info->object = nullptr;
		delete info;
	};

	// PluginManager 先行：~PluginManager 释放 Plugin 对象（FreeLibrary/dlclose）
	destroyInfo(pluginInfo);

	// ComponentManager 次之：~ComponentManager 会 delete 所有 Component*，包括其自身的封装，
	// 以及 ObjectManager 的封装；因此 ObjectManager 的 ObjectInfo 在此后不再使用 component。
	destroyInfo(compInfo);

	// ObjectManager 本身：此时所有 ComponentInfo* 的 deletor 可能指向 ComponentManager
	// 已释放的 Component 封装，因此直接销毁对象实例与 ObjectInfo，而不经 deletor。
	if (objMgrInfo != nullptr) {
		// 对 ObjectManager 自身，直接 delete（它就是 _objManager）
		delete objMgrInfo;
	}
	delete _objManager;
	_objManager = nullptr;
	_componentManager = nullptr;
}

PCX_END_NAMESPACE