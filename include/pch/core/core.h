/**
 * @file core.h
 * @brief 主机侧包含：动态加载 PCH 库并通过 pch::api 调用能力接口
 * @details
 * 常规用法：只需包含此文件，调用 pch::api::Initialize(libraryPath) 等。
 * 在运行时加载 PCH 动态库，然后按名称查找函数
 *（如 Initialize）并调用它们。通常不需要预先链接 PCH 的 .lib。
 *
 * 如果您的项目与 PCH 主程序静态链接，且希望直接在代码中
 * 调用 Initialize、CreateObject 等函数，可在 coreexport.h 末尾声明它们；
 * 那里的返回值为 int，与本处的 ErrorCode（本质为 int）匹配。
 */
#pragma once
#include <stdarg.h>
#include "coreexport.h"
#include "error.h"
#include "interface.h"
#include "plugininfo.h"
#include "componentinfo.h"

PCH_BEGIN_NAMESPACE

// api 单例类，将 PCH 能力封装为一组静态函数
class api
{
public:
	/**
	* @brief 初始化 PCH 平台
	*
	* @param  pchPath PCH 库文件路径
	* @param  objectManager 将返回 objectManager 对象
	*
	* @return 错误码 PCH_SUCCESS 表示成功
	*         PCH_FAILED 初始化失败，详情见日志
	*/
	static inline ErrorCode Initialize(const char* pchPath = PCH_LIB_NAME)
	{
		auto& instance = api::get();

		// 幂等：与 core 侧 Initialize 语义一致，重复初始化视为成功
		if (instance._objectManager)
			return PCH_SUCCESS;

		// 从动态库中查找 Initialize 符号，转换为函数指针并保存
		instance._initializeFunc = (InitializeFunc)__linkto(pchPath, "Initialize");
		// 类似地查找 Terminate 符号，保存到 _terminateFunc
		instance._terminateFunc = (TerminateFunc)__linkto(pchPath, "Terminate");

		// 如果任一入口缺失，则库不完整或加载失败
		if (!instance._initializeFunc || !instance._terminateFunc)
			return PCH_FAILED;

		// 调用 PCH 的初始化函数，创建 PCH 对象
		return instance._initializeFunc(instance._objectManager);
	}

	/**
	* @brief 创建匿名组件对象（无名，无法按名称查找，调用方需删除）
	* @param componentID  - 组件 ID
	* @param  initMsg     初始化消息参数，使用 initMsg 作为创建时的参数包
	* @param  errCode     错误码
	* @param  file         函数执行的源文件名
	* @param  line         函数执行的源文件行号
	* @return 成功返回 void* 指针，失败返回 nullptr（使用 errCode 获取详细错误信息）
	* @note initMsg 仅作为初始化消息投递，由调用方负责释放（与 sendMessage 不同）
	*/
	static inline void *CreateObject(const char *componentID, IMessage* initMsg = nullptr, ErrorCode *errCode = nullptr, const char* file = nullptr, int line = 0)
	{
		auto& instance = api::get();
		if (!instance._objectManager) {
			if (errCode != nullptr)
				*errCode = PCH_FAILED;
			return nullptr;
		}
		return instance._objectManager->createObject(componentID, initMsg, errCode, file, line);
	}

	/**
	* @brief 创建全局组件对象，对象名为 objName。如果 objName 已存在则失败。
	* 注意：全局对象由平台释放
	* @param  componentID   组件 ID
	* @param  objName       对象名
	* @param  initMsg       初始化消息参数，使用 initMsg 作为创建时的参数包
	* @param  errCode       错误码
	* @param  file          函数执行的源文件名
	* @param  line          函数执行的源文件行号
	*
	* @return 成功返回 void* 指针，失败返回 nullptr（名称已存在或内存不足，使用 errCode）
	* @note initMsg 仅作为初始化消息投递，由调用方负责释放（与 sendMessage 不同）
	*/
	static inline void *CreateNamedObject(const char *componentID, const char* objName, IMessage* initMsg = nullptr, ErrorCode *errCode = nullptr, const char* file = nullptr, int line = 0)
	{
		auto& instance = api::get();
		if (!instance._objectManager) {
			if (errCode != nullptr)
				*errCode = PCH_FAILED;
			return nullptr;
		}
		return instance._objectManager->createNamedObject(componentID, objName, initMsg, errCode, file, line);
	}

	/**
	* @brief 按名称查找组件对象
	*
	* @param  objName   对象名
	*
	* @return 对象实例地址，未找到返回 nullptr
	*/
	static inline void *FindObject(const char *objName)
	{
		auto& instance = api::get();
		if (!instance._objectManager)
			return nullptr;

		return instance._objectManager->findObject(objName);
	}

	/**
	* @brief 按指针删除匿名组件对象
	*
	* @param  obj        组件对象指针
	* @param  file       函数执行的源文件名
	* @param  line       函数执行的源文件行号
	*
	* @return 成功返回 PCH_SUCCESS，失败返回错误码
	*/
	static inline ErrorCode DeleteObject(void* obj, const char* file = nullptr, int line = 0)
	{
		auto& instance = api::get();
		if (!instance._objectManager)
			return PCH_FAILED;

		return instance._objectManager->deleteObject(obj, file, line);
	}

	/**
	* @brief 停止 PCH 并清理所有组件对象
	* @return 成功返回 PCH_SUCCESS
	*/
	static inline ErrorCode Terminate()
	{
		auto& instance = api::get();
		if (!instance._terminateFunc)
			return PCH_FAILED;

		ErrorCode err = instance._terminateFunc();   // 停止 PCH 并释放内部资源
		instance._objectManager = nullptr;           // 清除对象管理器指针，避免悬空引用
		instance._initializeFunc = nullptr;          // 清除 Initialize 函数指针
		instance._terminateFunc = nullptr;           // 清除 Terminate 函数指针
		__unlinkfrom();                              // 释放 __linkto 缓存的库句柄，允许重新加载
		return err;                                  // 返回终止结果
	}

	/** 进程单例；插件 pluginit（见 plugininfo.h）需要访问以注册 IObjectManager* */
	static api& get()
	{
		static api _api;
		return _api;
	}

protected:
	IObjectManager* _objectManager = nullptr; // 由 Initialize / pluginit 写入

private:
	typedef ErrorCode(*InitializeFunc)(IObjectManager* &objectManager);
	typedef ErrorCode(*TerminateFunc)();

	api() = default;

	InitializeFunc  _initializeFunc = nullptr;
	TerminateFunc   _terminateFunc  = nullptr;
};

PCH_END_NAMESPACE
