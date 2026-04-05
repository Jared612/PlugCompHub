#include <string.h>
#include <algorithm>
#include "messagecenter.h"
#include "interface.h"
#include "mcpcomponent.h"
#include "mcpplugin.h"
#include <error.h>
#include "objectManager.h"
#include "loggerManager.h"
#include "environment.h"

MCP_REGISTER_COMPONENT(mcp::MessageCenter, MCP_MESSAGECENTER_ID)

MCP_BEGIN_NAMESPACE

extern ObjectManager* _objManager;  //全局对象管理器实例指针，由mcp初始化流程维护
extern Environment* _environment;   //全局环境，用于解析中继对象名等。

/**
 * @brief 构造并启动后台投递线程。
 * @details `_thread` 立即以 `MessageCenter::run` 为入口运行：
 */
MessageCenter::MessageCenter():
	_isTerminate(false),
	_thread(std::make_unique<std::thread>(&MessageCenter::run, this))  // run(this)：异步队列消费线程
{
}

/**
 * @brief 请求后台投递线程退出并 join。
 * @details 置退出标志、唤醒条件变量并 join 后台投递线程，避免僵尸线程。
 */
MessageCenter::~MessageCenter()
{
	{
		std::unique_lock<std::mutex> lk(_mutex);
		_isTerminate = true;
		_cv.notify_all();
	}

	if (_thread && _thread->joinable()) {
		_thread->join();
	}
}

/**
 * @brief 创建 `Message`；`size > 0` 时分配零填充负载缓冲区。
 * @param[in] code 消息业务码
 * @param[in] size 数据字节数（可为 0）
 * @param[in] sender 发送端对象名，可为 nullptr
 * @return 成功返回 `IMessage*`；分配失败返回 nullptr
 */
IMessage* MessageCenter::allocMessage(uint32_t code, uint32_t size, const char* sender)
{
	// 创建 `Message` 对象
	mcp::Message* msg = new mcp::Message();
	// 设置数据长度
	msg->_dataLen = size;
	// 设置发送端对象名
	if (sender != nullptr) {
		msg->_sender = sender;
	}
	// 设置数据为空
	msg->_data = nullptr;
	// 设置消息业务码
	msg->_code = code;
	// 如果数据长度大于 0，则分配数据缓冲区
	if (size > 0) {
		msg->_data = new (std::nothrow) unsigned char[size];
		// 如果分配失败，则删除 `Message` 对象并返回 nullptr
		if (msg->_data == nullptr) {
			delete msg;
			return nullptr;
		}
	}
	// 如果数据长度大于 0，则填充数据缓冲区
    if (msg->_data) {
		memset(msg->_data, 0, size);
	}
	// 返回 `Message` 对象
	return msg;
}

/**
 * @brief 创建 `Message`；`data` 非空时把 `dataLen` 字节拷入缓冲区。
 * @param[in] code 消息业务码
 * @param[in] sender 发送端对象名，可为 nullptr
 * @param[in] dataLen 数据长度；缓冲区按此长度分配
 * @param[in] data 源数据指针；为 nullptr 时仅分配空间不拷贝
 * @param[in] type 消息类型
 * @return 成功返回 `IMessage*`；分配失败返回 nullptr
 */
IMessage* mcp::MessageCenter::allocMessage(uint32_t code, const char* sender, uint32_t dataLen, void* data, uint32_t type)
{
	// 创建 `Message` 对象
	mcp::Message* msg = new mcp::Message();
	// 设置数据长度为 0
	msg->_dataLen = 0;
	// 设置消息业务码
	msg->_code = code;
	// 设置消息类型
	msg->_type = type;
	// 设置发送端对象名
	if (sender != nullptr) {
		msg->_sender = sender;
	}
	// 如果数据长度大于 0，则分配数据缓冲区并拷贝数据
	if (dataLen > 0) {
		// 分配数据缓冲区
		msg->_data = new (std::nothrow) unsigned char[dataLen];
		// 如果分配失败，则删除 `Message` 对象并返回 nullptr
		if (msg->_data == nullptr) {
			delete msg;
			return nullptr;
		}
		// 如果数据非空，则拷贝数据到数据缓冲区
		if (data) {
			// 设置数据长度为 dataLen
			msg->_dataLen = dataLen;
			// 拷贝数据到数据缓冲区
			std::copy_n((char*)data, dataLen, msg->_data);
		}
	}
	// 返回 `Message` 对象
	return msg;
}

/**
 * @brief 释放负载缓冲区与 `Message` 对象。
 * @param[in] msg 待释放的 `IMessage*`，可为 nullptr
 * @return `MCP_SUCCESS` 或 `MCP_NULLPTR`
 */
ErrorCode MessageCenter::freeMessage(IMessage* msg)
{
	// 如果消息不为空，则释放数据缓冲区并删除 `Message` 对象
	if (msg) {
		// 如果数据不为空
		if (msg->getData()) {
			// 释放数据缓冲区
			delete[] msg->getData();
		}			
		// 删除 `Message` 对象
		delete msg;
		// 返回 `MCP_SUCCESS`
		return MCP_SUCCESS;
	}
	// 返回 `MCP_NULLPTR`
	return MCP_NULLPTR;
}

/**
 * @brief 本地同步派发或远程同步发送；成功时释放 `request`。
 * @param[in] target 目标对象名
 * @param[in,out] request 请求消息；成功时由本实现释放并置 nullptr
 * @param[out] response 非空时接收应答；调用方负责 `freeMessage`
 * @param[in] dstNodeID 目标节点，0 表示本进程
 * @param[in] timeout 超时（毫秒），远程路径使用
 * @return `MCP_SUCCESS` 或错误码
 */
mcp::ErrorCode mcp::MessageCenter::sendMessage(const char* target, IMessage*& request, IMessage** response, uint32_t dstNodeID , uint32_t timeout)
{	
	// 如果目标对象名为空，则返回 `MCP_PARAM_INVALID`
	if (target == nullptr || strcmp(target, "") == 0) {
		// 写入日志
		WriteLog(McpLogLevel::Debug, "Target is nullptr or empty");
		return MCP_PARAM_INVALID;
	}
	// 如果请求消息为空，则返回 `MCP_PARAM_INVALID`
	if (request == nullptr) {
		// 写入日志
		WriteLog(McpLogLevel::Debug, "Message is nullptr");
		return MCP_PARAM_INVALID;
	}
	// 如果对象管理器为空，则返回 `MCP_OBJMANAGER_NULLPTR`
	if (nullptr == _objManager) {
		// 写入日志
		WriteLog(McpLogLevel::Debug, "g_objectManager is nullptr");
		return MCP_OBJMANAGER_NULLPTR;
    }

	// 如果目标节点 ID 不为 0，则走 `sendRemoteMessage`
	if (0 != dstNodeID) {
		// 走 `sendRemoteMessage`
		return sendRemoteMessage(dstNodeID,target,request,response,timeout);
	}
	
	// 本进程的消息：调用 `invokeHandler` 派发消息
	int ret = invokeHandler(_objManager->getObjInfo(target), request, response);
	// 如果派发成功，则释放请求消息
	if (MCP_SUCCESS == ret) {
		// 释放请求消息
		freeMessage(request);
		request = nullptr;	
    }
	return ret;
}

/**
 * @brief 对 `group` 内名字依次同步 `invokeHandler`，成功则释放 `message`。
 * @param[in] group 目标对象名数组
 * @param[in] count 数组长度
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @param[in] msgPolicy 顺序与遇错策略
 * @param[in] timeout 预留（当前实现未使用）
 * @return `MCP_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::multicastLocalMessage(const char* group[], int count, IMessage* &message, uint32_t msgPolicy, uint32_t timeout)
{
	// 如果消息为空，则返回 `MCP_PARAM_INVALID`
	if (message == nullptr)
		return MCP_PARAM_INVALID;

    // 如果对象管理器为空，则返回 `MCP_OBJMANAGER_NULLPTR`
    if (nullptr == _objManager)
	{
		WriteLog(McpLogLevel::Error, "g_objectManager is nullptr");
		return MCP_OBJMANAGER_NULLPTR;
    }
	// 根据策略决定是否逆序
	bool reverse = (msgPolicy & ReverseOrder);
	// 根据策略决定是否遇到错误就停止
	bool breakError = (msgPolicy & BreakOnError);
	// 初始化返回码
	ErrorCode ret = MCP_SUCCESS;
	// 如果不逆序，则遍历目标对象名数组
	if (!reverse)
	{
		// 遍历目标对象名数组
		for (size_t i = 0; i < count; i++)
		{
			IMessage* msg = nullptr;
			// 调用 `invokeHandler` 派发消息
			ret = invokeHandler(_objManager->getObjInfo(group[i]), message, &msg);
			if (msg)
				freeMessage(msg);
			// 如果遇到错误且不是消息处理者未找到，则返回错误码
			if (breakError && ret != MCP_SUCCESS)
				return ret;
		}
	}
	else
	{
		// 如果逆序，则遍历目标对象名数组
		for (int i = count - 1; i >= 0; i--)
		{
			IMessage* msg = nullptr;
			// 调用 `invokeHandler` 派发消息
			ret = invokeHandler(_objManager->getObjInfo(group[i]), message, &msg);
			// 如果响应消息不为空，则释放响应消息
			if (msg)
				freeMessage(msg);
			// 如果遇到错误且不是消息处理者未找到，则返回错误码
			if (breakError && ret != MCP_SUCCESS)
				return ret;
		}
	}
	// 如果返回码为 `MCP_SUCCESS`，则释放消息
	if (MCP_SUCCESS == ret)
	{
		freeMessage(message);
		message = nullptr;
	}
	return ret;
}

/**
 * @brief `dstNodeID==0` 为本地异步广播，否则远程异步广播。
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @param[in] dstNodeID 目标节点，0 表示本进程
 * @param[in] msgPolicy 顺序与遇错策略
 * @return `MCP_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::broadcastMessageAsync(IMessage*& message, uint32_t dstNodeID , uint32_t msgPolicy )
{
	// 本地异步广播，走 `broadcastLocalMessageAsync`
	if (0 == dstNodeID) {
		return broadcastLocalMessageAsync(message, msgPolicy);
	}
	// 远程异步广播，走 `broadcastRemoteMessageAsync`
	return broadcastRemoteMessageAsync(dstNodeID, message);
}

/**
 * @brief 遍历 `_objManager` 已注册对象同步派发，最后释放 `message`。
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @param[in] msgPolicy 顺序与遇错策略
 * @param[in] timeout 预留（当前实现未使用）
 * @return `MCP_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::broadcastLocalMessage(IMessage* &message, uint32_t msgPolicy, uint32_t timeout)
{
	// 如果对象管理器不为空，则遍历已注册对象同步派发
	if (_objManager)
	{	
		// 根据策略决定是否逆序
		bool reverse = (msgPolicy & ReverseOrder);
		// 根据策略决定是否遇到错误就停止
		bool breakError = (msgPolicy & BreakOnError);
		// 初始化返回码
		ErrorCode ret = MCP_SUCCESS;
		// 获取已注册对象列表
		auto objList = _objManager->getRegisterObjects();
		// 如果不逆序，则遍历已注册对象列表
		if (!reverse)
		{
			// 遍历已注册对象列表
			for (const auto& obj : objList)
			{
				IMessage* msg = nullptr;
				// 调用 `invokeHandler` 派发消息
				ret = invokeHandler(obj, message, &msg);
				// 如果响应消息不为空，则释放响应消息
				if (msg)
					freeMessage(msg);
				// 如果遇到错误且不是消息处理者未找到，则返回错误码
				if (breakError && ret != MCP_SUCCESS && ret != MCP_MESSAGEHANDLER_NOTFOUND)
				{
					return ret;
				}
			}
		}
		// 如果逆序，则遍历已注册对象列表
		else
		{
			// 遍历已注册对象列表
			for (auto it = objList.rbegin(); it != objList.rend(); ++it)
			{
				IMessage* msg = nullptr;
				// 调用 `invokeHandler` 派发消息
				ret = invokeHandler(*it, message, &msg);
				// 如果响应消息不为空，则释放响应消息
				if (msg)
					freeMessage(msg);
				// 如果遇到错误且不是消息处理者未找到，则返回错误码
				if (breakError && ret != MCP_SUCCESS && ret != MCP_MESSAGEHANDLER_NOTFOUND)
				{
					return ret;
				}
			}
		}
	}
	else
	{
		WriteLog(McpLogLevel::Debug, "g_objectManager is nullptr!");
		return MCP_OBJMANAGER_NULLPTR;
	}
	// 释放消息
	freeMessage(message);
	message = nullptr;
	return MCP_SUCCESS;
}

/**
 * @brief 本地入队单目标或 `postRemoteMessage`。
 * @param[in] target 目标对象名
 * @param[in,out] message 待投递消息；返回前由本实现释放并置 nullptr
 * @param[in] dstNodeID 目标节点，0 表示本进程入队
 * @return `MCP_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::postMessage(const char* target, IMessage*& message, uint32_t dstNodeID )
{
	// 如果目标对象名为空，则返回错误码
	if (target == nullptr || strcmp(target, "") == 0 || message == nullptr) {
		return MCP_PARAM_INVALID;
	}

	// 如果对象管理器为空，则返回错误码	
    if (nullptr == _objManager) {
		return MCP_OBJMANAGER_NULLPTR;
	}

	// 分布式消息，走 `postRemoteMessage`
	if (0 != dstNodeID) {
		return postRemoteMessage(dstNodeID, target, message);
	}
	
	// 本进程的消息：获取对象信息，如果获取成功，则入队；如果获取失败，则返回错误码
	if (ObjectInfo* objPtr = _objManager->getObjInfo(target)) {
		enqueueMessage({objPtr}, message);
		return MCP_SUCCESS;
	}
	else {
		return MCP_OBJECT_NOTFOUND;
	}
}

/**
 * @brief 解析 `group` 为 `ObjectInfo*` 列表并入队异步投递。
 * @param[in] group 目标对象名数组
 * @param[in] count 数组长度
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @param[in] msgPolicy 本地路径下的顺序等策略位（见 `ReverseOrder`/`BreakOnError`）
 * @return `MCP_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::multicastLocalMessageAsync(const char* group[], int count, IMessage* &message, uint32_t msgPolicy)
{
	// 如果消息为空，则返回错误码
	if (message == nullptr)
		return MCP_PARAM_INVALID;

    // 如果对象管理器为空，则返回错误码
	if (nullptr == _objManager)
	{
		WriteLog(McpLogLevel::Debug, "g_objectManager is nullptr");
		return MCP_OBJMANAGER_NULLPTR;
	}

	// 根据策略决定是否逆序
	bool reverse = (msgPolicy & ReverseOrder);

	// 初始化返回码
	ErrorCode ret = MCP_SUCCESS;
	// 初始化目标对象列表
	std::vector<ObjectInfo*> objs;
	if (!reverse)
	{	
		// 遍历目标对象名数组
		for (size_t i = 0; i < count; i++)
		{
			if (group[i] == nullptr || strcmp(group[i], "") == 0)
			{
				// 如果对象名称为空，则跳过
				continue;
			}
			// 获取对象信息，如果获取失败，则返回错误码
			if (ObjectInfo* objPtr = _objManager->getObjInfo(group[i]))
			{
				// 如果获取成功，则加入目标对象列表
				objs.push_back(objPtr);
			}
			else {
				// 如果获取失败，则设置返回码并退出循环
				ret = MCP_OBJECT_NOTFOUND;
				break;
			}
		}
	} 
	else {
		// 遍历目标对象名数组
		for (int i = count - 1; i >= 0; i--) 
		{
			// 如果对象名称为空，则跳过	
			if (group[i] == nullptr || strcmp(group[i], "") == 0) {
				continue;
			}
			// 获取对象信息，如果获取失败，则返回错误码
			if (ObjectInfo* objPtr = _objManager->getObjInfo(group[i])) {
				objs.push_back(objPtr);
			} else {
				ret = MCP_OBJECT_NOTFOUND;
			}
		}
	}

	enqueueMessage(objs, message);
	return ret;
}

/**
 * @brief 将全部已注册对象（可按策略逆序）入队。
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @param[in] msgPolicy 本地路径下的顺序等策略位（见 `ReverseOrder`/`BreakOnError`）
 * @return `MCP_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::broadcastLocalMessageAsync(IMessage* &message, uint32_t msgPolicy)
{
	// 如果消息为空，则返回错误码
	if (message == nullptr)
		return MCP_PARAM_INVALID;

	// 根据策略决定是否逆序
	bool reverse = (msgPolicy & ReverseOrder);

	// 如果对象管理器为空，则返回错误码
	ErrorCode ret = MCP_SUCCESS;
	if (_objManager) {
		auto objLists = _objManager->getRegisterObjects();
		// 如果不逆序，则直接入队	
		if (!reverse) {
			// 入队
			enqueueMessage(objLists, message);
		} else {
			// 如果逆序，则反转对象列表
			std::reverse(objLists.begin(), objLists.end());
			// 入队
			enqueueMessage(objLists, message);
		}
	} 
	else {
		// 如果对象管理器为空，则返回错误码
		WriteLog(McpLogLevel::Debug, "ObjectManager is nullptr!");
		ret = MCP_OBJMANAGER_NULLPTR;
	}

	// 如果广播失败，则返回错误码
	if (ret != MCP_SUCCESS && ret != MCP_MESSAGEHANDLER_NOTFOUND) {
		return ret;
	} else {
		return MCP_SUCCESS;
	}
}

/**
 * @brief 工作线程：等待队列、对每条 `MessagePack` 逐个目标派发并释放消息。
 * @details 等待队列、对每条 `MessagePack` 逐个目标派发并释放消息。
 */
void MessageCenter::run()
{
	// 循环直到退出
	while (!_isTerminate) {
		// 加锁后检查队列是否为空
		std::unique_lock<std::mutex> lk(_mutex);
		// 如果队列为空，则等待队列非空或退出
		if (_messages.empty()) {
			_cv.wait(lk, [=]() { return !_messages.empty() || _isTerminate; });
		}
		// 如果队列不为空，则取出队首的 `MessagePack`
		if (!_messages.empty()) {
			MessagePack msgPack = std::move(_messages.front());
			// 从队列中移除队首的 `MessagePack`
			_messages.pop_front();
			// 解锁
			lk.unlock();
			// 遍历目标对象列表
			for (const auto& target : msgPack._target) {
				// 调用 `invokeHandler` 派发消息
				IMessage* msg = nullptr;
				// 调用 `invokeHandler` 派发消息
				invokeHandler(target, (IMessage*&)msgPack._msg, &msg);
				if (msg) {
					freeMessage((IMessage*)msg);
				}
			}
			// 释放消息
			if (msgPack._msg) {
				freeMessage((IMessage*)msgPack._msg);
				// 将消息置 nullptr
				msgPack._msg = nullptr;
			}
		}
	}	
}

/**
 * @brief 从目标组件取 `IMessageHandler` 并调用 `handleMessage`，按需回填 `response` 或释放返回值。
 * @param[in] objInfo 目标 `ObjectInfo*`
 * @param[in,out] req 请求消息；返回前由本实现释放并置 nullptr
 * @param[out] response 非空时接收应答；调用方负责 `freeMessage`
 * @return `MCP_SUCCESS`、`MCP_OBJECT_NOTFOUND`、`MCP_MESSAGEHANDLER_NOTFOUND` 等
 */
ErrorCode MessageCenter::invokeHandler(ObjectInfo* objInfo, IMessage* &req, IMessage** response)
{
	// 如果对象管理器为空，则返回错误码
	if (nullptr == _objManager) {
		WriteLog(McpLogLevel::Debug, "g_objectManager is nullptr");
		return MCP_OBJMANAGER_NULLPTR;
	}
	
	// 如果目标对象为空，则返回错误码
	if (objInfo == nullptr) {
		return MCP_OBJECT_NOTFOUND;
	}

	// 如果目标对象组件、对象、请求消息都有效，则调用 `IMessageHandler::handleMessage`
	if (objInfo->component && objInfo->object && req) {
		IMessageHandler* componentTmpl = (IMessageHandler*)objInfo->component->getComponentInfo()->getMessageHandler(objInfo->object);
		if (componentTmpl) {
			// 调用 `IMessageHandler::handleMessage`
			const IMessage* msg = componentTmpl->handleMessage(req);
			// 如果响应消息非空，则将响应消息赋值给 `response`
			if (response) 
				*response = (IMessage*)msg;
			// 如果响应消息为空，则释放响应消息
			else if (msg)
				freeMessage((IMessage*)msg);
			// 返回 `MCP_SUCCESS`
			return MCP_SUCCESS;
		} 
		// 如果 `IMessageHandler` 为空，则返回错误码
		else {
			// 返回 `MCP_MESSAGEHANDLER_NOTFOUND`
			return MCP_MESSAGEHANDLER_NOTFOUND;
		}
	} 
	// 如果目标对象为空，则返回错误码
	else {
		// 如果目标对象为空，则返回错误码
		std::string reason = "";
		if (objInfo == nullptr || objInfo->component == nullptr) {
			reason.append("[Target objectInfo is invalid]");
		}
		// 如果目标对象对象为空，则返回错误码
		if (objInfo->object == nullptr) {
			reason.append("[Target has been deleted!]");
		}
		// 如果请求消息为空，则返回错误码
		if (req == nullptr) {
			reason.append("[Message is nullptr]");
		}
		WriteLog(McpLogLevel::Debug, "The parameters to invoke 'onMessageCome' is invaild!, detail:%s", reason.c_str());
		return MCP_PARAM_INVALID;
	}
}

/**
 * @brief 组装 `MessagePack` 入队并 `notify_one`，调用方 `msg` 置空。
 * @param[in] targets 目标对象 `ObjectInfo*` 列表
 * @param[in,out] msg 待入队消息；返回前由本实现释放并置 nullptr
 */
void mcp::MessageCenter::enqueueMessage(std::vector<ObjectInfo*> targets, IMessage* &msg)
{
	// 组装 `MessagePack`
	MessagePack msgpack{targets, msg};
	msg = nullptr;
	{
		// 加锁后入队
		std::unique_lock<std::mutex> lk(_mutex);
		_messages.emplace_back(msgpack);
	}
	// 唤醒线程
	_cv.notify_one();
}

/**
 * @brief 委托 `IMessageRelayer::sendMessage`，随后释放 `request`。
 * @param[in] dstNodeID 目标节点 ID
 * @param[in] target 目标对象名
 * @param[in,out] request 请求消息；返回前由本实现释放并置 nullptr
 * @param[out] response 非空时接收应答；调用方负责 `freeMessage`
 * @param[in] timeout 超时（毫秒）
 * @return Relayer 或本层检查结果
 */
ErrorCode mcp::MessageCenter::sendRemoteMessage(uint32_t dstNodeID, const char* target, IMessage* &request, IMessage** response, uint32_t timeout)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr)
	{
		WriteLog(McpLogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(request);
		request = nullptr;
		return MCP_FAILED;
	}
	// 转发 `IMessageRelayer::sendMessage`
	ErrorCode errCode = _messageRelayer->sendMessage(dstNodeID, target, request, response, timeout);
	// 释放 `request`
	freeMessage(request);
	// 将 `request` 置 nullptr
	request = nullptr;
	return errCode;
}

/**
 * @brief 委托 `IMessageRelayer::postMessage`，随后释放 `message`。
 * @param[in] dstNodeID 目标节点 ID
 * @param[in] target 目标对象名
 * @param[in,out] message 待投递消息；返回前由本实现释放并置 nullptr
 * @return Relayer 或本层检查结果
 */
ErrorCode mcp::MessageCenter::postRemoteMessage(uint32_t dstNodeID, const char* target, IMessage* &message)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr)
	{
		WriteLog(McpLogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(message);
		message = nullptr;
		return MCP_FAILED;
	}
	// 转发 `IMessageRelayer::postMessage`
	ErrorCode errCode = _messageRelayer->postMessage(dstNodeID, target, message);
	// 释放 `message`
	freeMessage(message);
	// 将 `message` 置 nullptr
	message = nullptr;
	return errCode;
}

/**
 * @brief 本地或远程组播异步入口。
 * @param[in] targets 目标对象名数组
 * @param[in] count 数组长度
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @param[in] dstNodeID 目标节点 ID
 * @param[in] msgPolicy 本地路径下的顺序等策略位（见 `ReverseOrder`/`BreakOnError`）
 * @return `MCP_SUCCESS` 或错误码
 */
ErrorCode mcp::MessageCenter::multicastMessageAsync(const char* targets[], int count, IMessage*& message, uint32_t dstNodeID , uint32_t msgPolicy)
{
	// 如果目标节点 ID 为 0，则本地组播
	if (0 == dstNodeID) {
		return multicastLocalMessageAsync(targets, count, message, msgPolicy);
	}
	// 转发 `IMessageRelayer::multicastRemoteMessageAsync`
	return multicastRemoteMessageAsync(dstNodeID, targets, count, message);
}

/**
 * @brief 委托 Relayer 组播异步，完成后释放 `message`。
 * @param[in] dstNodeID 目标节点 ID
 * @param[in] targets 目标对象名数组
 * @param[in] count 数组长度
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @return Relayer 或本层检查结果
 */
ErrorCode mcp::MessageCenter::multicastRemoteMessageAsync(uint32_t dstNodeID, const char* targets[], int count, IMessage* &message)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(McpLogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(message);
		message = nullptr;
		return MCP_FAILED;
	}
	// 转发 `IMessageRelayer::multicastRemoteMessageAsync`	
	ErrorCode errCode = _messageRelayer->multicastRemoteMessageAsync(dstNodeID, targets, count, message);
	// 释放 `message`
	freeMessage(message);
	// 将 `message` 置 nullptr
	message = nullptr;
	return errCode;
}

/**
 * @brief 委托 Relayer 广播异步，完成后释放 `message`。
 * @param[in] dstNodeID 目标节点 ID
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @return Relayer 或本层检查结果
 */
ErrorCode mcp::MessageCenter::broadcastRemoteMessageAsync(uint32_t dstNodeID, IMessage* &message)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(McpLogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(message);
		message = nullptr;
		return MCP_FAILED;
	}
	// 转发 `IMessageRelayer::broadcastRemoteMessageAsync`
	ErrorCode errCode = _messageRelayer->broadcastRemoteMessageAsync(dstNodeID, message);
	// 释放 `message`
	freeMessage(message);
	// 将 `message` 置 nullptr
	message = nullptr;
	return errCode;
}

/**
 * @brief 转发 `IMessageRelayer::joinGroup`。
 * @param[in] groupId 组 ID
 * @return `MCP_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::joinGroup(uint32_t groupId)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(McpLogLevel::Error, "_messageRelayer is nullptr");
		return MCP_FAILED;
	}
	// 转发 `IMessageRelayer::joinGroup`
	return _messageRelayer->joinGroup(groupId);
}

/**
 * @brief 转发 `IMessageRelayer::leaveGroup`。
 * @param[in] groupId 组 ID
 * @return `MCP_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::leaveGroup(uint32_t groupId)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(McpLogLevel::Error, "_messageRelayer is nullptr");
		return MCP_FAILED;
	}
	// 转发 `IMessageRelayer::leaveGroup`
	return _messageRelayer->leaveGroup(groupId);
}

/**
 * @brief 转发 `IMessageRelayer::getGroupMembers`。
 * @param[in] groupId 组 ID
 * @return 组成员列表；无中继时返回空串
 */
const char* MessageCenter::getGroupMembers(uint32_t groupId)
{
	// 如果中继对象为空，则返回空串
	if (_messageRelayer == nullptr) {
		WriteLog(McpLogLevel::Error, "_messageRelayer is nullptr");
		return "";
	}
	// 转发 `IMessageRelayer::getGroupMembers`
	return _messageRelayer->getGroupMembers(groupId);	
}

/**
 * @brief `SystemReady` 时从环境取中继对象名并在 `_objManager` 中查找 `_messageRelayer`。
 * @param[in] msg 系统消息
 * @return 成功返回 `IMessage*`；失败返回 nullptr
 */
const mcp::IMessage* MessageCenter::handleMessage(const mcp::IMessage* msg)
{
	// 如果消息为空，则返回 nullptr
	if (msg == nullptr){
		return nullptr;
	}

	// 根据消息类型进行处理
	switch (msg->getCode())
	{
		// 系统就绪消息
		case mcp::SystemReady:
		{
			// 从环境变量中获取中继对象名
			const char* name = _environment->get("MCP_MESSAGE_RELAY");
			if (name == nullptr || strlen(name) <= 0){
				name = "MCP_MESSAGE_RELAY";
			}
			// 在 `_objManager` 中查找 `_messageRelayer`
			_messageRelayer = (IMessageRelayer*)_objManager->findObject(name);
		}
		break;
	}
	return nullptr; // 返回 nullptr
}

MCP_END_NAMESPACE