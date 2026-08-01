/**
 * @file coreinit.cpp
 * @brief 内置核心组件装配：实现 pchcoreStart / pchcoreStop
 * @details 读取内置组件表，创建核心对象并注册到 ObjectManager 和 ComponentManager
 */

#include <assert.h>
#include <string.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "componentinfo.h"
#include "plugininfo.h"
#include "componentManager.h"
#include "messagecenter.h"
#include "objectManager.h"
#include "pluginManager.h"
#include "loggerManager.h"
#include "environment.h"

/**
 * @brief 内置组件导出表
 * @details
 * 内置组件导出表，用于 PCH 内核运行时的组件注册。
 * 启动时通过 PCH_GET_COMPONENT_TABLE 读取此表，参与核心对象装配。
 */	
PCH_COMPONENT_EXPORT_TABLE_BEGIN(_cmptable)             // 定义内置组件导出表 _cmptable
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::PluginManager)     // 注册插件管理器组件
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::ComponentManager)  // 注册组件管理器组件
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::ObjectManager)     // 注册对象管理器组件
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::MessageCenter)     // 注册消息中心组件
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::LoggerManager)     // 注册日志管理器组件
PCH_COMPONENT_EXPORT_TABLE_ITEM(pch::Environment)       // 注册环境管理器组件
PCH_COMPONENT_EXPORT_TABLE_END()                        // 结束内置组件导出表
PCH_BEGIN_NAMESPACE


extern ObjectManager* _objManager;			/** 声明（非定义）：_objManager 在 pch.cpp 中定义并分配存储，此处仅引用同一全局变量。 */
extern ComponentManager* _componentManager;	/** 同上，extern 表示链接到 pch.cpp 中的唯一定义。 */

/**
 * @brief 实现 pchcoreStart，启动 PCH 内核运行时
 * @return 成功返回 ObjectManager 接口，失败返回 nullptr
 */
__PCH_API IObjectManager* pchcoreStart()
{
	ComponentInfo** cmptable = PCH_GET_COMPONENT_TABLE(_cmptable);
	if (cmptable == nullptr) {
		WriteLog(LogLevel::Error, "The embedded table have troubles");
		return nullptr;
	}

	// 核心对象默认实例名称映射表
	std::unordered_map<std::string, std::string> comIDName = {
		{PCH_COMPONENT_MANAGER_ID, PCH_DEFAULT_COMPONENTMANAGER},
		{PCH_OBJECT_MANAGER_ID,    PCH_DEFAULT_OBJECTMANAGER},
		{PCH_PLUGIN_MANAGER_ID,    PCH_DEFAULT_PLUGINMANAGER},
		{PCH_MESSAGECENTER_ID,     PCH_DEFAULT_MESSAGECENTER},
		{PCH_LOGGERMANAGER_ID,     PCH_DEFAULT_LOGGERMANAGER},
		{PCH_ENVIROMENT_ID,        PCH_DEFAULT_ENVIROMENT}
	};

	// 记录 (ComponentInfo*, Component*) 配对关系；使用显式的 pair 容器而非
	// 两个共享索引 i 的数组，避免遇到 continue/跳过的项时索引错位
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
			WriteLog(LogLevel::Error, "componentID of cmptable[%d] is nullptr", i);
			continue;
		}
		if (ci->creator == nullptr || ci->deletor == nullptr) {
			WriteLog(LogLevel::Error, "Creator or deletor of component[%s] is nullptr", ci->componentID);
			continue;
		}

		Component* component = new (std::nothrow) Component(ci);
		if (component == nullptr) {
			WriteLog(LogLevel::Fatal, "alloc Component for [%s] failed", ci->componentID);
			continue;
		}
		ObjectInfo* objInfo = createObjectInfo(component, nullptr);
		if (objInfo == nullptr) {
			WriteLog(LogLevel::Fatal, "createObjectInfo for [%s] failed", ci->componentID);
			delete component;
			continue;
		}

		if (strcmp(ci->componentID, PCH_OBJECT_MANAGER_ID) == 0) {
			_objManager = static_cast<ObjectManager*>(objInfo->object);
		}
		objInfoMap[ci->componentID] = objInfo;
		entries.push_back({ ci, component, objInfo });
	}

	if (_objManager == nullptr) {
		WriteLog(LogLevel::Fatal, "ObjectManager is not built during pchcoreStart");
		// 回滚已创建的对象以避免泄漏（尚未注册到 ObjectManager，直接使用 ComponentInfo::deletor 释放）
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

	// 注册核心对象默认实例
	for (auto& kv : objInfoMap) {
		auto nameIt = comIDName.find(kv.first);
		if (nameIt == comIDName.end()) {
			continue;
		}
		_objManager->registerObj(kv.second, nameIt->second.c_str());
	}

	// 获取组件管理器对象
	auto cmIt = objInfoMap.find(PCH_COMPONENT_MANAGER_ID);
	if (cmIt == objInfoMap.end() || cmIt->second == nullptr) {
		WriteLog(LogLevel::Fatal, "ComponentManager is missing");
		return nullptr;
	}
	_componentManager = static_cast<ComponentManager*>(cmIt->second->object);
	if (_componentManager == nullptr) {
		WriteLog(LogLevel::Fatal, "ComponentManager object is nullptr");
		return nullptr;
	}

	// 将 ComponentInfo 与对应的 Component* 包装注册到 ComponentManager（不再依赖并行索引）
	for (auto& e : entries) {
		_componentManager->registerComponent(e.info, e.component);
	}

	return _objManager;
}

/**
 * @brief 实现 pchcoreStop，停止 PCH 内核运行时并释放核心管理器对象
 */
__PCH_API void pchcoreStop()
{
	if (_objManager == nullptr) {
		_componentManager = nullptr;
		return;
	}

	// 1) 先销毁业务对象和叶子管理器（LoggerManager/MessageCenter/Environment），
	//    同时保留 PluginManager/ComponentManager/ObjectManager 的 ObjectInfo
	_objManager->tearDown();

	// 2) 然后按顺序：PluginManager（卸载 DLL 使插件 ComponentInfo* 失效）、ComponentManager、
	//    最后 ObjectManager —— 销毁核心管理器自身
	ObjectInfo* pluginInfo = _objManager->getObjInfo(PCH_DEFAULT_PLUGINMANAGER);
	ObjectInfo* compInfo = _objManager->getObjInfo(PCH_DEFAULT_COMPONENTMANAGER);
	ObjectInfo* objMgrInfo = _objManager->getObjInfo(PCH_DEFAULT_OBJECTMANAGER);

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

	// 先销毁 PluginManager：~PluginManager 释放 Plugin 对象（FreeLibrary/dlclose）
	destroyInfo(pluginInfo);

	// 再销毁 ComponentManager：~ComponentManager 会 delete 所有 Component*，包括自身的 wrapper
	// 和 ObjectManager 的 wrapper；因此此后 ObjectManager 的 ObjectInfo 不再使用 component
	destroyInfo(compInfo);

	// ObjectManager 自身：此时所有 ComponentInfo* 和 deletor 可能指向 ComponentManager 中
	// 已释放的 Component wrapper，因此直接销毁对象实例和 ObjectInfo，不使用 deletor
	if (objMgrInfo != nullptr) {
		// 对于 ObjectManager 自身，直接删除（它就是 _objManager）
		delete objMgrInfo;
	}
	delete _objManager;
	_objManager = nullptr;
	_componentManager = nullptr;
}

PCH_END_NAMESPACE
