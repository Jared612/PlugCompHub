/**
 * @file mcpcore.cpp
 * @brief 内置核心组件装配：实现 `mcpcoreStart` / `mcpcoreStop`。
 * @details 读内置组件表、创建核心对象并注册到 `ObjectManager` 与 `ComponentManager`。
 */

#include <assert.h>
#include <string.h>
#include <string>
#include "mcpcomponent.h"
#include "mcpplugin.h"
#include "componentManager.h"
#include "messagecenter.h"
#include "objectManager.h"
#include "pluginManager.h"
#include "loggerManager.h"
#include "environment.h"

/**
 * @brief 内置组件导出表
 * @details
 * 内置组件导出表，用于在 MCP 核心运行时中注册组件。
 * 该表在启动阶段由 `MCP_GET_COMPONENT_TABLE` 读取并参与核心对象装配。
 */	
MCP_COMPONENT_EXPORT_TABLE_BEGIN(_cmptable)             // 定义内置组件导出表 _cmptable
MCP_COMPONENT_EXPORT_TABLE_ITEM(mcp::PluginManager)     // 注册插件管理器组件
MCP_COMPONENT_EXPORT_TABLE_ITEM(mcp::ComponentManager)  // 注册组件管理器组件
MCP_COMPONENT_EXPORT_TABLE_ITEM(mcp::ObjectManager)     // 注册对象管理器组件
MCP_COMPONENT_EXPORT_TABLE_ITEM(mcp::MessageCenter)     // 注册消息中心组件
MCP_COMPONENT_EXPORT_TABLE_ITEM(mcp::LoggerManager)     // 注册日志管理器组件
MCP_COMPONENT_EXPORT_TABLE_ITEM(mcp::Environment)       // 注册环境管理器组件
MCP_COMPONENT_EXPORT_TABLE_END()                        // 结束内置组件导出表

MCP_BEGIN_NAMESPACE


extern ObjectManager* _objManager;			/** 声明（非定义）：`_objManager` 在 `mcp.cpp` 中定义并分配存储，此处仅引用同一全局变量。 */
extern ComponentManager* _componentManager;	/** 同上，`extern` 表示链接到 `mcp.cpp` 中的唯一定义。 */

/**
 * @brief 实现 `mcpcoreStart`，启动 MCP 核心运行时。
 * @return 启动成功返回 ObjectManager 接口，失败返回 nullptr
 */
__MCP_API IObjectManager* mcpcoreStart()
{
	// 获取内置组件导出表并做基础校验	
	ComponentInfo** cmptable = MCP_GET_COMPONENT_TABLE(_cmptable);
	if (cmptable == nullptr) {
		WriteLog(McpLogLevel::Error, "The embedded table have troubles");
		return nullptr;
	}

	// 核心对象默认实例名映射表
	std::unordered_map<std::string, std::string> comIDName = {
		{MCP_COMPONENT_MANAGER_ID, MCP_DEFAULT_COMPONENTMANAGER},
		{MCP_OBJECT_MANAGER_ID, MCP_DEFAULT_OBJECTMANAGER},
		{MCP_PLUGIN_MANAGER_ID, MCP_DEFAULT_PLUGINMANAGER},
		{MCP_MESSAGECENTER_ID, MCP_DEFAULT_MESSAGECENTER},
		{MCP_LOGGERMANAGER_ID, MCP_DEFAULT_LOGGERMANAGER},
		{MCP_ENVIROMENT_ID, MCP_DEFAULT_ENVIROMENT }};

	// 核心对象信息映射表
	std::unordered_map<std::string, ObjectInfo*> objInfoMap;

	// 组件对象临时存储容器
	std::vector<Component*> tempVec;

	// 遍历内置组件导出表，创建组件对象并注册到 ObjectManager
	for (int i = 0; cmptable[i] != nullptr; i++) {
		// 检查组件 ID 是否为空
		if (nullptr == cmptable[i]->componentID) {
			WriteLog(McpLogLevel::Error, "componentID of cmptable[%d] is nullptr", i);
			continue;
        }
		
		// 检查组件创建器和销毁器是否为空
		if (cmptable[i]->creator == nullptr || cmptable[i]->deletor == nullptr) {
			WriteLog(McpLogLevel::Error, "Creator or deletor of component[%s] is nullptr", cmptable[i]->componentID);
		}

		// 创建组件对象
		Component *component = new Component(cmptable[i]);
		// 将组件对象添加到临时存储容器
		tempVec.push_back(component);
		// 创建对象信息
		ObjectInfo* objInfo = createObjectInfo(component, nullptr);
		// 检查对象信息是否为空
		assert(objInfo != nullptr);
		// 如果组件 ID 为 MCP_OBJECT_MANAGER_ID，则将对象管理器指针设置为当前对象
		if (strcmp(cmptable[i]->componentID, MCP_OBJECT_MANAGER_ID) == 0) {
			_objManager = static_cast<ObjectManager*>(objInfo->object);
		}
		// 将对象信息添加到对象信息映射表
		objInfoMap[cmptable[i]->componentID] = objInfo;
	}

	// 遍历对象信息映射表，注册核心对象默认实例名
	assert(_objManager != nullptr);
	for (auto it = objInfoMap.begin(); it != objInfoMap.end(); it++) {
		_objManager->registerObj(it->second, comIDName[it->first].c_str());
	}

	// 获取组件管理器对象
	_componentManager = static_cast<ComponentManager*>(objInfoMap[MCP_COMPONENT_MANAGER_ID]->object);
	assert(_componentManager != nullptr);

	// 遍历临时存储容器，将组件信息注册进 ComponentManager
	for (int i = 0; cmptable[i] != nullptr; i++) {
		_componentManager->registerComponent(cmptable[i], tempVec[i]);
	}
	
	// 返回对象管理器指针
	return _objManager;
}

/**
 * @brief 实现 `mcpcoreStop`，停止 MCP 核心运行时并释放核心管理对象。
 */
__MCP_API void mcpcoreStop()
{
	// 执行全局对象下线流程
	if (_objManager) {
		_objManager->tearDown();
		delete _objManager;
		_objManager = nullptr;
	}

	// 清空组件管理器指针
	_componentManager = nullptr;
}

MCP_END_NAMESPACE