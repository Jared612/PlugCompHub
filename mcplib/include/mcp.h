/**
 * @file mcp.h
 * @brief 宿主侧**常规用法**：通过 `mcp::api` 动态加载 MCP 库并调用能力接口。
 * @details
 * 常规做法：只包含本文件，调用 `mcp::api::Initialize(库路径)` 等。运行时会**加载 MCP 动态库**，再按**函数名**
 *（如 `Initialize`）在库里查找地址并调用，一般**不必**在工程里预先链接 MCP 的导入库。
 *
 * 若你的工程是**把 MCP 和主程序编译链接在一起**，要在代码里**直接写** `Initialize`、`CreateObject` 等，
 * 则声明在 `mcpexport.h` 文件**最后一段**；那里返回值写成 `int`，与本文件里的 `ErrorCode`（本质上就是整数）一致。
 */
#pragma once
#include <stdarg.h>
#include "mcpexport.h"
#include "error.h"
#include "interface.h"
#include "mcpplugin.h"
#include "mcpcomponent.h"

MCP_BEGIN_NAMESPACE

// api单例类，作用是把 MCP 的全局能力封装成一组静态函数。
class api
{
public:
	/**
	* @brief 初始化MCP平台
	*
	* @param  mcpPath mcp库文件路径
	* @param  objectManager 会返回objectManager对象
	*
	* @return 错误码 MCP_SUCCESS 成功
	*			MCP_FAILED 初始化失败，详情见日志
	*/
	static inline ErrorCode Initialize(const char *mcpPath = __MCP_NAME)
	{
		auto& instance = api::get();

		// 如果对象管理器已经存在，则返回失败
		if (instance._objectManager)
			return MCP_FAILED;

		// 从动态库里找名为 Initialize 的符号，转成函数指针保存到 _initializeFunc
		instance._initializeFunc = (InitializeFunc)__linkto(mcpPath, "Initialize");
		// 同样找 Terminate 符号，保存到 _terminateFunc
		instance._terminateFunc = (TerminateFunc)__linkto(mcpPath, "Terminate");

		//只要任意一个入口没找到，就认为库不完整或加载失败
		if (!instance._initializeFunc || !instance._terminateFunc)
			return MCP_FAILED;

		//调用ncp的初始化函数，创建ncp对象
		return instance._initializeFunc(instance._objectManager);
	}

	/**
	* @brief 创建匿名组件对象，此对象没有名称，不能通过名称查找，需要用户自行删除
	* @param componentID	- 组件ID
	* @param  initMsg       初始化消息参数，你要给组件一个“创建时参数包”，就用 initMsg；
	* @param  errCode       错误码
	* @param  file			函数执行时所在文件名
	* @param  line			函数执行时所在文件行数
	* @return 创建成功返回对象void*指针，
	*         创建失败返回nullptr，使用errCode获得详细错误信息
	*/
	static inline void *CreateObject(const char *componentID, IMessage* initMsg = nullptr, ErrorCode *errCode = nullptr, const char* file = nullptr, int line = 0)
	{
		auto& instance = api::get();
		if (!instance._objectManager) {
			if (errCode != nullptr)
				*errCode = MCP_FAILED;
			return nullptr;
		}
		return instance._objectManager->createObject(componentID, initMsg, errCode, file, line);
	}

	/**
	* @brief 创建全局组件对象，对象名称为objName，若objName已存在会创建失败
	*	！全局对象由平台释放
	* @param  componentID	 组件ID
	* @param  objName       对象名称
	* @param  initMsg       初始化消息参数，你要给组件一个“创建时参数包”，就用 initMsg；
	* @param  errCode       错误码
	* @param  file			函数执行时所在文件名
	* @param  line			函数执行时所在文件行数
	*
	* @return 创建成功返回对象void*指针，
	*         对象名已存在或内存不够等创建失败返回nullptr，使用errCode获得详细错误信息
	*/
	static inline void *CreateNamedObject(const char *componentID, const char* objName, IMessage* initMsg = nullptr, ErrorCode *errCode = nullptr, const char* file = nullptr, int line = 0)
	{
		auto& instance = api::get();
		if (!instance._objectManager) {
			if (errCode != nullptr)
				*errCode = MCP_FAILED;
			return nullptr;
		}
		return instance._objectManager->createNamedObject(componentID, objName, initMsg, errCode, file, line);
	}

	/**
	* @brief 查找组件对象
	*
	* @param  objName   对象名称
	*
	* @return 对象实例地址，获取失败返回nullptr
	*/
	static inline void *FindObject(const char *objName)
	{
		auto& instance = api::get();
		if (!instance._objectManager)
			return nullptr;

		return instance._objectManager->findObject(objName);
	}

	/**
	* @brief 通过匿名组件对象指针删除对象
	*
	* @param  obj       组件对象指针
	* @param  file		函数执行时所在文件名
	* @param  line		函数执行时所在文件行数
	*
	* @return MCP_SUCCESS 删除成功
	*         其他         错误码
	*/
	static inline ErrorCode DeleteObject(void* obj, const char* file = nullptr, int line = 0)
	{
		auto& instance = api::get();
		if (!instance._objectManager)
			return MCP_FAILED;

		return instance._objectManager->deleteObject(obj, file, line);
	}

	/**
	* @brief 停止MCP并清理所有组件对象
	* @return MCP_SUCCESS 成功
	*/
	static inline ErrorCode Terminate()
	{
		auto& instance = api::get();
		if (!instance._terminateFunc)
			return MCP_FAILED;

		ErrorCode err = instance._terminateFunc();   // 停止 MCP 运行时并释放内部资源
		instance._objectManager = nullptr;           // 清空对象管理器指针，避免悬空引用
		instance._initializeFunc = nullptr;          // 清空 Initialize 函数指针
		instance._terminateFunc = nullptr;           // 清空 Terminate 函数指针
		return err;                                  // 返回终止过程结果
	}

	/** 进程内单例；插件 `pluginit`（见 `mcpplugin.h`）需访问以注入 `IObjectManager*`。 */
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

MCP_END_NAMESPACE
