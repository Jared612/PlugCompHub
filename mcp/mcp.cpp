/**
 * @file mcp.cpp
 * @brief `extern "C"` 导出入口实现（声明在 `mcpexport.h` 文末）；供与 MCP 链接时直接调用。
 * @details `Initialize`/`Terminate` 调 `mcpcoreStart`/`mcpcoreStop`；对象相关接口转 `ObjectManager`。动态加载场景用 `mcp.h` 即可。
 */
#include <string.h>
#include <fstream>
#include "componentManager.h"
#include "error.h"
#include "interface.h"
#include "messagecenter.h"
#include "mcpcomponent.h"
#include "mcpexport.h"
#include "mcpplugin.h"
#include "objectManager.h"
#include "pluginManager.h"
#include "loggerManager.h"
#include "environment.h"

MCP_BEGIN_NAMESPACE

ObjectManager*    _objManager       = nullptr;  // 全局对象管理器指针
PluginManager*    _pluginManager    = nullptr;  // 全局插件管理器指针
ComponentManager* _componentManager = nullptr;  // 全局组件管理器指针
MessageCenter*    _msgCenter        = nullptr;  // 全局消息中心指针
LoggerManager*    _loggerManager    = nullptr;  // 全局日志管理器指针
Environment*      _environment      = nullptr;  // 全局环境组件指针

/**
 * @brief 前向声明：启动 MCP 核心（实现在 `mcpcore.cpp`）。
 * @details
 * 此处 `extern` 仅表示**链接单元外部的定义**，供本文件调用 `mcpcoreStart()`；
 * 与 `mcp.cpp` 中其它函数同属 `mcp` 目标，由 `mcpcore.cpp` 提供函数体。
 * @return 成功返回 `ObjectManager*`（以 `IObjectManager*` 形式）；失败返回 `nullptr`。
 */
extern __MCP_API IObjectManager* mcpcoreStart();

/**
 * @brief 前向声明：停止 MCP 核心并释放核心资源（实现在 `mcpcore.cpp`）。
 * @details
 * 同为**外部声明**，定义在 `mcpcore.cpp` 的 `mcpcoreStop()`，供 `Terminate()` 等调用。
 */
extern __MCP_API void mcpcoreStop();


/**
 * @brief 实现 `Initialize`，启动 MCP 核心并返回对象管理器接口。
 * @param[out] objectManager 对象管理器指针引用，由 `mcpcoreStart()` 设置
 * @return 成功返回 `MCP_SUCCESS`，失败返回 `MCP_FAILED`
 */
ErrorCode Initialize(IObjectManager* &objectManager)
{
	printf("****************************************************************\n");
	printf("*                                                              *\n");
	printf("*                  Welcome! MCP is Starting...                 *\n");
	printf("*                                                              *\n");
	printf("****************************************************************\n\n");

	// 调用 mcpcoreStart() 启动 MCP 核心并获取对象管理器
	_objManager = (ObjectManager*)mcpcoreStart();
	if (_objManager == nullptr) {
		WriteLog(McpLogLevel::Error,"MCP coreplugin initialize failed!");
		// 如果对象管理器为空，则返回失败
		return MCP_FAILED;
	}

	// 按默认对象名查找并缓存核心组件对象：PluginManager、ComponentManager、MessageCenter、LoggerManager、Environment
	_pluginManager    = static_cast<PluginManager*>(_objManager->findObject(MCP_DEFAULT_PLUGINMANAGER));
	_componentManager = static_cast<ComponentManager*>(_objManager->findObject(MCP_DEFAULT_COMPONENTMANAGER));
	_msgCenter        = static_cast<MessageCenter*>(_objManager->findObject(MCP_DEFAULT_MESSAGECENTER));
	_loggerManager    = static_cast<LoggerManager*>(_objManager->findObject(MCP_DEFAULT_LOGGERMANAGER));
	_environment      = static_cast<Environment*>(_objManager->findObject(MCP_DEFAULT_ENVIROMENT));

	// 如果核心组件对象查找失败，则返回 MCP_FAILED
	if (_pluginManager == nullptr || _componentManager == nullptr || _msgCenter == nullptr || _loggerManager == nullptr || _environment == nullptr)
	{
		WriteLog(McpLogLevel::Error, "MCP core component find failed!");
		return MCP_FAILED;
	}

	WriteLog(McpLogLevel::Information, "MCP initialize finished!");

	// 返回对象管理器指针
	objectManager = _objManager;		
	return MCP_SUCCESS;
}


/**
 * @brief 实现 `CreateNamedObject`，创建具名全局对象。
 * @param[in] componentID 组件 ID
 * @param[in] objName 对象名称
 * @param[in] initMsg 初始化消息（可选）
 * @param[out] errCode 错误码输出指针（可选）
 * @param[in] file 调用文件名（可选，用于调试）
 * @param[in] line 调用行号（可选，用于调试）
 * @return 创建成功返回对象指针，失败返回 nullptr
 */
void* CreateNamedObject(const char* componentID, const char* objName, IMessage* initMsg, ErrorCode* errCode, const char* file, int line)
{	
	// 如果组件 ID 或对象名称是空指针或空字符串，则返回 nullptr
	if (componentID == nullptr || strcmp(componentID, "") == 0 || objName == nullptr || strcmp(objName, "") == 0) {
		WriteLog(McpLogLevel::Debug, "Parameter is nullptr when CreateObject");
		return nullptr;
	}

	// 如果对象管理器不为空，则调用 _objManager->createNamedObject() 创建具名对象
	if (_objManager) {
		return _objManager->createNamedObject(componentID, objName, initMsg, errCode, file, line);
	} 
	else {
		// 如果对象管理器为空，则设置错误码为 MCP_OBJMANAGER_NULLPTR 并返回 nullptr
		WriteLog(McpLogLevel::Debug, "Object Manager is nullptr, may be MCP not initialized or failed!");
		if (errCode) {
			*errCode = MCP_OBJMANAGER_NULLPTR;
		}
		return nullptr;
	}
}

/**
 * @brief 实现 `CreateObject`，创建匿名全局对象。
 * @param[in] componentID 组件 ID
 * @param[in] initMsg 初始化消息（可选）
 * @param[out] errCode 错误码输出指针（可选）
 * @param[in] file 调用文件名（可选，用于调试）
 * @param[in] line 调用行号（可选，用于调试）
 * @return 创建成功返回对象指针，失败返回 nullptr
 */
void* CreateObject(const char* componentID, IMessage* initMsg, ErrorCode* errCode, const char* file, int line)
{
	// 如果组件 ID 是空指针或空字符串，则返回 nullptr
	if (componentID == nullptr || strcmp(componentID, "") == 0) {
		WriteLog(McpLogLevel::Debug, "ComponentID is nullptr or empty when CreateObject");
		return nullptr;
	}

	// 如果对象管理器不为空，则调用 _objManager->createObject() 创建匿名对象
	if (_objManager) {
		void* p = _objManager->createObject(componentID, initMsg, errCode, file, line);
		return p;
	} 
	else {
		// 如果对象管理器为空，则设置错误码为 MCP_OBJMANAGER_NULLPTR 并返回 nullptr
		WriteLog(McpLogLevel::Debug, "Object Manager is nullptr, may be MCP not initialized or failed!");
		if (errCode) {
			*errCode = MCP_OBJMANAGER_NULLPTR;
		}
		return nullptr;
	}
}

/**
 * @brief 实现 `FindObject`，按对象名查找全局对象。
 * @param[in] objName 对象名称
 * @return 查找成功返回对象指针，失败返回 nullptr
 */
void* FindObject(const char* objName)
{
	// 如果对象名称是空指针或空字符串，则返回 nullptr
	if (objName == nullptr || strcmp(objName, "") == 0) {
		WriteLog(McpLogLevel::Debug, "ObjName is nullptr when FindObject");
		return nullptr;
	}

	// 如果对象管理器不为空，则调用 _objManager->findObject() 按对象名查找对象
	if (_objManager) {
		return _objManager->findObject(objName);	
	} 
	// 如果对象管理器为空，则设置错误码为 MCP_OBJMANAGER_NULLPTR 并返回 nullptr
	else {
		WriteLog(McpLogLevel::Debug, "Object Manager is nullptr, may be MCP not initialized or failed!");
		return nullptr;
	}
}

/**
 * @brief 实现 `DeleteObject`，通过匿名对象指针删除对象。
 * @param[in] obj 对象指针
 * @param[in] file 调用文件名（可选，用于调试）
 * @param[in] line 调用行号（可选，用于调试）
 * @return 删除成功返回 MCP_SUCCESS，失败返回错误码
 */	
ErrorCode DeleteObject(void* obj, const char* file, int line)
{
	// 如果对象指针为空，则返回 MCP_PARAM_NULLPTR
	if (obj == nullptr) {
		return MCP_PARAM_NULLPTR;
	}
	
	// 调用 _objManager->deleteObject() 删除对象
	if (_objManager) {
		return _objManager->deleteObject(obj, file, line);
	}
	else {
		// 如果对象管理器为空，则设置错误码为 MCP_OBJMANAGER_NULLPTR 并返回 MCP_OBJMANAGER_NULLPTR
		WriteLog(McpLogLevel::Debug, "Object Manager is nullptr, may be MCP not initialized or failed!");
		return MCP_OBJMANAGER_NULLPTR;
	}
}

/**
 * @brief 实现 `Terminate`，停止 MCP 核心并清理所有对象。
 * @return 成功返回 MCP_SUCCESS，失败返回 MCP_FAILED
 */
ErrorCode Terminate()
{
	printf("****************************************************************\n");
	printf("*                                                              *\n");
	printf("*                  Bye! MCP is terminating...                  *\n");
	printf("*                                                              *\n");
	printf("****************************************************************\n\n");

	// 调用 mcpcoreStop() 停止 MCP 核心并清空全局管理器指针
	mcpcoreStop();

	_objManager       = nullptr;  // 清空对象管理器指针
	_pluginManager    = nullptr;  // 清空插件管理器指针
	_componentManager = nullptr;  // 清空组件管理器指针

	// 返回 MCP_SUCCESS
	return MCP_SUCCESS;
}

MCP_END_NAMESPACE