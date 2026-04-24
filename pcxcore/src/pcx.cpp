/**
 * @file pcx.cpp
 * @brief `extern "C"` 导出入口实现（声明在 `pcxexport.h` 文末）；供与 PCX 链接时直接调用。
 * @details `Initialize`/`Terminate` 调 `pcxcoreStart`/`pcxcoreStop`；对象相关接口转 `ObjectManager`。动态加载场景用 `pcx.h` 即可。
 */
#include <string.h>
#include <fstream>
#include "componentManager.h"
#include "error.h"
#include "interface.h"
#include "messagecenter.h"
#include "pcxcomponent.h"
#include "pcxexport.h"
#include "pcxplugin.h"
#include "objectManager.h"
#include "pluginManager.h"
#include "loggerManager.h"
#include "environment.h"

PCX_BEGIN_NAMESPACE

ObjectManager*    _objManager       = nullptr;  // 全局对象管理器指针
PluginManager*    _pluginManager    = nullptr;  // 全局插件管理器指针
ComponentManager* _componentManager = nullptr;  // 全局组件管理器指针
MessageCenter*    _msgCenter        = nullptr;  // 全局消息中心指针
LoggerManager*    _loggerManager    = nullptr;  // 全局日志管理器指针
Environment*      _environment      = nullptr;  // 全局环境组件指针

/**
 * @brief 前向声明：启动 PCX 核心（实现在 `pcxcore.cpp`）。
 * @details
 * 此处 `extern` 仅表示**链接单元外部的定义**，供本文件调用 `pcxcoreStart()`；
 * 与 `pcx.cpp` 中其它函数同属 `pcx` 目标，由 `pcxcore.cpp` 提供函数体。
 * @return 成功返回 `ObjectManager*`（以 `IObjectManager*` 形式）；失败返回 `nullptr`。
 */
extern __PCX_API IObjectManager* pcxcoreStart();

/**
 * @brief 前向声明：停止 PCX 核心并释放核心资源（实现在 `pcxcore.cpp`）。
 * @details
 * 同为**外部声明**，定义在 `pcxcore.cpp` 的 `pcxcoreStop()`，供 `Terminate()` 等调用。
 */
extern __PCX_API void pcxcoreStop();


/**
 * @brief 实现 `Initialize`，启动 PCX 核心并返回对象管理器接口。
 * @param[out] objectManager 对象管理器指针引用，由 `pcxcoreStart()` 设置
 * @return 成功返回 `PCX_SUCCESS`，失败返回 `PCX_FAILED`
 */
ErrorCode Initialize(IObjectManager* &objectManager)
{
	// 幂等保护：已经 Initialize 过且未 Terminate 时不再重复启动，避免覆盖旧全局指针导致资源泄漏与已注册对象成为孤儿
	if (_objManager != nullptr) {
		WriteLog(PcxLogLevel::Warning, "PCX has already been initialized; skipping duplicate Initialize");
		objectManager = _objManager;
		return PCX_SUCCESS;
	}

	printf("****************************************************************\n");
	printf("*                                                              *\n");
	printf("*                  Welcome! PCX is Starting...                 *\n");
	printf("*                                                              *\n");
	printf("****************************************************************\n\n");

	// 调用 pcxcoreStart() 启动 PCX 核心并获取对象管理器
	_objManager = (ObjectManager*)pcxcoreStart();
	if (_objManager == nullptr) {
		WriteLog(PcxLogLevel::Error,"PCX coreplugin initialize failed!");
		// 如果对象管理器为空，则返回失败
		return PCX_FAILED;
	}

	// 按默认对象名查找并缓存核心组件对象：PluginManager、ComponentManager、MessageCenter、LoggerManager、Environment
	_pluginManager    = static_cast<PluginManager*>(_objManager->findObject(PCX_DEFAULT_PLUGINMANAGER));
	_componentManager = static_cast<ComponentManager*>(_objManager->findObject(PCX_DEFAULT_COMPONENTMANAGER));
	_msgCenter        = static_cast<MessageCenter*>(_objManager->findObject(PCX_DEFAULT_MESSAGECENTER));
	_loggerManager    = static_cast<LoggerManager*>(_objManager->findObject(PCX_DEFAULT_LOGGERMANAGER));
	_environment      = static_cast<Environment*>(_objManager->findObject(PCX_DEFAULT_ENVIROMENT));

	// 如果核心组件对象查找失败，则返回 PCX_FAILED
	if (_pluginManager == nullptr || _componentManager == nullptr || _msgCenter == nullptr || _loggerManager == nullptr || _environment == nullptr)
	{
		WriteLog(PcxLogLevel::Error, "PCX core component find failed!");
		return PCX_FAILED;
	}

	WriteLog(PcxLogLevel::Information, "PCX initialize finished!");

	// 返回对象管理器指针
	objectManager = _objManager;		
	return PCX_SUCCESS;
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
	// 参数校验：组件 ID / 对象名不能为空
	if (componentID == nullptr || componentID[0] == '\0' || objName == nullptr || objName[0] == '\0') {
		WriteLog(PcxLogLevel::Debug, "CreateNamedObject: invalid parameter");
		if (errCode) {
			*errCode = PCX_PARAM_INVALID;
		}
		return nullptr;
	}

	if (_objManager == nullptr) {
		WriteLog(PcxLogLevel::Debug, "CreateNamedObject: ObjectManager is nullptr, PCX may not be initialized");
		if (errCode) {
			*errCode = PCX_OBJMANAGER_NULLPTR;
		}
		return nullptr;
	}

	return _objManager->createNamedObject(componentID, objName, initMsg, errCode, file, line);
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
	if (componentID == nullptr || componentID[0] == '\0') {
		WriteLog(PcxLogLevel::Debug, "CreateObject: componentID is nullptr or empty");
		if (errCode) {
			*errCode = PCX_PARAM_INVALID;
		}
		return nullptr;
	}

	if (_objManager == nullptr) {
		WriteLog(PcxLogLevel::Debug, "CreateObject: ObjectManager is nullptr, PCX may not be initialized");
		if (errCode) {
			*errCode = PCX_OBJMANAGER_NULLPTR;
		}
		return nullptr;
	}

	return _objManager->createObject(componentID, initMsg, errCode, file, line);
}

/**
 * @brief 实现 `FindObject`，按对象名查找全局对象。
 * @param[in] objName 对象名称
 * @return 查找成功返回对象指针，失败返回 nullptr
 */
void* FindObject(const char* objName)
{
	if (objName == nullptr || objName[0] == '\0') {
		WriteLog(PcxLogLevel::Debug, "FindObject: objName is nullptr or empty");
		return nullptr;
	}
	if (_objManager == nullptr) {
		WriteLog(PcxLogLevel::Debug, "FindObject: ObjectManager is nullptr, PCX may not be initialized");
		return nullptr;
	}
	return _objManager->findObject(objName);
}

/**
 * @brief 实现 `DeleteObject`，通过匿名对象指针删除对象。
 * @param[in] obj 对象指针
 * @param[in] file 调用文件名（可选，用于调试）
 * @param[in] line 调用行号（可选，用于调试）
 * @return 删除成功返回 PCX_SUCCESS，失败返回错误码
 */	
ErrorCode DeleteObject(void* obj, const char* file, int line)
{
	if (obj == nullptr) {
		return PCX_PARAM_NULLPTR;
	}
	if (_objManager == nullptr) {
		WriteLog(PcxLogLevel::Debug, "DeleteObject: ObjectManager is nullptr, PCX may not be initialized");
		return PCX_OBJMANAGER_NULLPTR;
	}
	return _objManager->deleteObject(obj, file, line);
}

/**
 * @brief 实现 `Terminate`，停止 PCX 核心并清理所有对象。
 * @return 成功返回 PCX_SUCCESS，失败返回 PCX_FAILED
 */
ErrorCode Terminate()
{
	printf("****************************************************************\n");
	printf("*                                                              *\n");
	printf("*                  Bye! PCX is terminating...                  *\n");
	printf("*                                                              *\n");
	printf("****************************************************************\n\n");

	// 调用 pcxcoreStop() 停止 PCX 核心并清空全局管理器指针
	pcxcoreStop();

	// pcxcoreStop 已经销毁了所有核心管理器实例，此处必须把全部全局指针清零，
	// 避免 `Terminate` 后再次被调用或日志宏访问导致的 UAF。
	_objManager       = nullptr;
	_pluginManager    = nullptr;
	_componentManager = nullptr;
	_msgCenter        = nullptr;
	_loggerManager    = nullptr;
	_environment      = nullptr;

	return PCX_SUCCESS;
}

PCX_END_NAMESPACE