/**
 * @file coreapi.cpp
 * @brief extern "C" 导出入口实现（在 coreexport.h 末尾声明）；用于直接链接 PCH
 * @details Initialize/Terminate 调用 pchcoreStart/pchcoreStop；对象相关接口委托给 ObjectManager。动态加载场景只需使用 core.h。
 */
#include <string.h>
#include "componentManager.h"
#include "error.h"
#include "interface.h"
#include "messagecenter.h"
#include "componentinfo.h"
#include "coreexport.h"
#include "plugininfo.h"
#include "objectManager.h"
#include "pluginManager.h"
#include "loggerManager.h"
#include "environment.h"

PCH_BEGIN_NAMESPACE

ObjectManager*    _objManager       = nullptr;  // 全局对象管理器指针
PluginManager*    _pluginManager    = nullptr;  // 全局插件管理器指针
ComponentManager* _componentManager = nullptr;  // 全局组件管理器指针
MessageCenter*    _msgCenter        = nullptr;  // 全局消息中心指针
LoggerManager*    _loggerManager    = nullptr;  // 全局日志管理器指针
Environment*      _environment      = nullptr;  // 全局环境组件指针

/**
 * @brief 前向声明：启动 PCH 内核（在 coreinit.cpp 中实现）
 * @details
 * 此 extern 仅表示此翻译单元外有定义，供本文件调用 pchcoreStart()。
 * 本文件中的其他函数同样位于 PCH 目标中；coreinit.cpp 提供函数体。
 * @return 成功返回 ObjectManager*（作为 IObjectManager*）；失败返回 nullptr
 */
extern __PCH_API IObjectManager* pchcoreStart();

/**
 * @brief 前向声明：停止 PCH 内核并释放核心资源（在 coreinit.cpp 中实现）
 * @details
 * 也是外部声明；定义在 coreinit.cpp 中的 pchcoreStop()，由 Terminate() 等调用
 */
extern __PCH_API void pchcoreStop();


/**
 * @brief 实现 Initialize，启动 PCH 内核并返回对象管理器接口
 * @param[out] objectManager 对象管理器指针引用，由 pchcoreStart() 设置
 * @return 成功返回 PCH_SUCCESS，失败返回 PCH_FAILED
 */
ErrorCode Initialize(IObjectManager* &objectManager)
{
	// 幂等性保护：如果已初始化且未 Terminate，不重新启动，避免覆盖旧的全局指针并导致孤立注册对象造成资源泄漏
	if (_objManager != nullptr) {
		WriteLog(LogLevel::Warning, "PCH has already been initialized; skipping duplicate Initialize");
		objectManager = _objManager;
		return PCH_SUCCESS;
	}

	printf("****************************************************************\n");
	printf("*                                                              *\n");
	printf("*                  Welcome! PCH is Starting...                 *\n");
	printf("*                                                              *\n");
	printf("****************************************************************\n\n");

	// 调用 pchcoreStart() 启动 PCH 内核并获取对象管理器
	_objManager = (ObjectManager*)pchcoreStart();
	if (_objManager == nullptr) {
		WriteLog(LogLevel::Error,"PCH coreplugin initialize failed!");
		// 如果对象管理器为空，返回失败
		return PCH_FAILED;
	}

	// 按默认对象名称查找并缓存核心组件对象：PluginManager、ComponentManager、MessageCenter、LoggerManager、Environment
	_pluginManager    = static_cast<PluginManager*>(_objManager->findObject(PCH_DEFAULT_PLUGINMANAGER));
	_componentManager = static_cast<ComponentManager*>(_objManager->findObject(PCH_DEFAULT_COMPONENTMANAGER));
	_msgCenter        = static_cast<MessageCenter*>(_objManager->findObject(PCH_DEFAULT_MESSAGECENTER));
	_loggerManager    = static_cast<LoggerManager*>(_objManager->findObject(PCH_DEFAULT_LOGGERMANAGER));
	_environment      = static_cast<Environment*>(_objManager->findObject(PCH_DEFAULT_ENVIROMENT));

	// 如果核心组件对象查找失败，返回 PCH_FAILED
	if (_pluginManager == nullptr || _componentManager == nullptr || _msgCenter == nullptr || _loggerManager == nullptr || _environment == nullptr)
	{
		WriteLog(LogLevel::Error, "PCH core component find failed!");
		// 清理已启动的内核，避免失败后留下半初始化状态
		pchcoreStop();
		_pluginManager    = nullptr;
		_componentManager = nullptr;
		_msgCenter        = nullptr;
		_loggerManager    = nullptr;
		_environment      = nullptr;
		return PCH_FAILED;
	}

	WriteLog(LogLevel::Information, "PCH initialize finished!");

	// 返回对象管理器指针
	objectManager = _objManager;		
	return PCH_SUCCESS;
}


/**
 * @brief 实现 CreateNamedObject，创建命名全局对象
 * @param[in] componentID 组件 ID
 * @param[in] objName 对象名称
 * @param[in] initMsg 初始化消息（可选）
 * @param[out] errCode 错误码输出指针（可选）
 * @param[in] file 调用方文件名（可选，用于调试）
 * @param[in] line 调用方行号（可选，用于调试）
 * @return 成功返回对象指针，失败返回 nullptr
 */
void* CreateNamedObject(const char* componentID, const char* objName, IMessage* initMsg, ErrorCode* errCode, const char* file, int line)
{
	// 参数校验：组件 ID / 对象名不能为空
	if (componentID == nullptr || componentID[0] == '\0' || objName == nullptr || objName[0] == '\0') {
		WriteLog(LogLevel::Debug, "CreateNamedObject: invalid parameter");
		if (errCode) {
			*errCode = PCH_PARAM_INVALID;
		}
		return nullptr;
	}

	if (_objManager == nullptr) {
		WriteLog(LogLevel::Debug, "CreateNamedObject: ObjectManager is nullptr, PCH may not be initialized");
		if (errCode) {
			*errCode = PCH_OBJMANAGER_NULLPTR;
		}
		return nullptr;
	}

	return _objManager->createNamedObject(componentID, objName, initMsg, errCode, file, line);
}

/**
 * @brief 实现 CreateObject，创建匿名全局对象
 * @param[in] componentID 组件 ID
 * @param[in] initMsg 初始化消息（可选）
 * @param[out] errCode 错误码输出指针（可选）
 * @param[in] file 调用方文件名（可选，用于调试）
 * @param[in] line 调用方行号（可选，用于调试）
 * @return 成功返回对象指针，失败返回 nullptr
 */
void* CreateObject(const char* componentID, IMessage* initMsg, ErrorCode* errCode, const char* file, int line)
{
	if (componentID == nullptr || componentID[0] == '\0') {
		WriteLog(LogLevel::Debug, "CreateObject: componentID is nullptr or empty");
		if (errCode) {
			*errCode = PCH_PARAM_INVALID;
		}
		return nullptr;
	}

	if (_objManager == nullptr) {
		WriteLog(LogLevel::Debug, "CreateObject: ObjectManager is nullptr, PCH may not be initialized");
		if (errCode) {
			*errCode = PCH_OBJMANAGER_NULLPTR;
		}
		return nullptr;
	}

	return _objManager->createObject(componentID, initMsg, errCode, file, line);
}

/**
 * @brief 实现 FindObject，按对象名查找全局对象
 * @param[in] objName 对象名称
 * @return 成功返回对象指针，失败返回 nullptr
 */
void* FindObject(const char* objName)
{
	if (objName == nullptr || objName[0] == '\0') {
		WriteLog(LogLevel::Debug, "FindObject: objName is nullptr or empty");
		return nullptr;
	}
	if (_objManager == nullptr) {
		WriteLog(LogLevel::Debug, "FindObject: ObjectManager is nullptr, PCH may not be initialized");
		return nullptr;
	}
	return _objManager->findObject(objName);
}

/**
 * @brief 实现 DeleteObject，按匿名对象指针删除对象
 * @param[in] obj 对象指针
 * @param[in] file 调用方文件名（可选，用于调试）
 * @param[in] line 调用方行号（可选，用于调试）
 * @return 成功返回 PCH_SUCCESS，失败返回错误码
 */	
ErrorCode DeleteObject(void* obj, const char* file, int line)
{
	if (obj == nullptr) {
		return PCH_PARAM_NULLPTR;
	}
	if (_objManager == nullptr) {
		WriteLog(LogLevel::Debug, "DeleteObject: ObjectManager is nullptr, PCH may not be initialized");
		return PCH_OBJMANAGER_NULLPTR;
	}
	return _objManager->deleteObject(obj, file, line);
}

/**
 * @brief 实现 Terminate，停止 PCH 内核并清理所有对象
 * @return 成功返回 PCH_SUCCESS，失败返回 PCH_FAILED
 */
ErrorCode Terminate()
{
	printf("****************************************************************\n");
	printf("*                                                              *\n");
	printf("*                  Bye! PCH is terminating...                  *\n");
	printf("*                                                              *\n");
	printf("****************************************************************\n\n");

	// 调用 pchcoreStop() 停止 PCH 内核并清除全局管理器指针
	pchcoreStop();

	// pchcoreStop 已销毁所有核心管理器实例；此处必须将所有全局指针置零
	// 避免后续调用或 Terminate 后的日志宏访问导致的 UAF
	_objManager       = nullptr;
	_pluginManager    = nullptr;
	_componentManager = nullptr;
	_msgCenter        = nullptr;
	_loggerManager    = nullptr;
	_environment      = nullptr;

	return PCH_SUCCESS;
}

PCH_END_NAMESPACE
