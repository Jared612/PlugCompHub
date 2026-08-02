#include <string.h>
#include <algorithm>
#include "messagecenter.h"
#include "interface.h"
#include "componentinfo.h"
#include "plugininfo.h"
#include "error.h"
#include "objectManager.h"
#include "loggerManager.h"
#include "environment.h"

PCH_REGISTER_COMPONENT(pch::MessageCenter, PCH_MESSAGECENTER_ID)

PCH_BEGIN_NAMESPACE

extern ObjectManager* _objManager;  // 全局对象管理器实例指针，由 PCH 初始化流程维护
extern Environment* _environment;   // 全局环境变量，用于解析中继对象名等

/**
 * @brief 构造并启动后台分发线程
 * @details _thread 立即以 MessageCenter::run 为入口运行：
 */
MessageCenter::MessageCenter():
	_isTerminate(false),
	_thread(std::make_unique<std::thread>(&MessageCenter::run, this))  // run(this)：异步队列消费者线程
{
}

/**
 * @brief 请求后台分发线程退出并 join，清理未投递的队列，避免消息泄漏
 * @details 设置退出标志 => 唤醒工作线程 => join；然后 freeMessage _messages 中所有剩余的 MessagePack，
 *          确保析构函数不会留下未释放的消息缓冲区
 */
MessageCenter::~MessageCenter()
{
	{
		std::unique_lock<std::mutex> lk(_mutex);
		_isTerminate.store(true, std::memory_order_release);
		_cv.notify_all();
	}

	if (_thread && _thread->joinable()) {
		_thread->join();
	}

	// 线程退出后，无人再访问 _messages；此处无需加锁
	for (auto& pack : _messages) {
		if (pack._msg) {
			freeMessage(pack._msg);
			pack._msg = nullptr;
		}
	}
	_messages.clear();
}

/**
 * @brief 创建 Message；当 size > 0 时，分配零填充的载荷缓冲区
 * @param[in] code 消息业务码
 * @param[in] size 数据字节数（可为 0）
 * @param[in] sender 发送者对象名，可为 nullptr
 * @return 成功返回 IMessage*；分配失败返回 nullptr
 */
IMessage* MessageCenter::allocMessage(uint32_t code, uint32_t size, const char* sender)
{
	// 创建 Message 对象
	pch::Message* msg = new pch::Message();
	// 设置数据长度
	msg->_dataLen = size;
	// 设置发送者对象名
	if (sender != nullptr) {
		msg->_sender = sender;
	}
	// 设置数据为空
	msg->_data = nullptr;
	// 设置消息业务码
	msg->_code = code;
	// 如果数据长度 > 0，分配数据缓冲区
	if (size > 0) {
		msg->_data = new (std::nothrow) unsigned char[size];
		// 如果分配失败，删除 Message 对象并返回 nullptr
		if (msg->_data == nullptr) {
			delete msg;
			return nullptr;
		}
		// 填充数据缓冲区
		memset(msg->_data, 0, size);
	}
	// 返回 Message 对象
	return msg;
}

/**
 * @brief 创建 Message；当 data 非空时，拷贝 dataLen 字节到缓冲区
 * @param[in] code 消息业务码
 * @param[in] sender 发送者对象名，可为 nullptr
 * @param[in] dataLen 数据长度；按此长度分配缓冲区
 * @param[in] data 源数据指针；为 nullptr 时只分配空间不拷贝
 * @param[in] type 消息类型
 * @return 成功返回 IMessage*；分配失败返回 nullptr
 */
IMessage* pch::MessageCenter::allocMessage(uint32_t code, const char* sender, uint32_t dataLen, void* data, uint32_t type)
{
	// 创建 Message 对象
	pch::Message* msg = new pch::Message();
	// 设置数据长度为 0
	msg->_dataLen = 0;
	// 设置消息业务码
	msg->_code = code;
	// 设置消息类型
	msg->_type = type;
	// 设置发送者对象名
	if (sender != nullptr) {
		msg->_sender = sender;
	}
	if (dataLen > 0) {
		msg->_data = new (std::nothrow) unsigned char[dataLen];
		if (msg->_data == nullptr) {
			delete msg;
			return nullptr;
		}
		// 无论 data 是否为 null，分配成功后都登记长度，确保 getSize() 与缓冲区匹配
		msg->_dataLen = dataLen;
		if (data != nullptr) {
			std::copy_n((char*)data, dataLen, msg->_data);
		} else {
			memset(msg->_data, 0, dataLen);
		}
	}
	return msg;
}

/**
 * @brief 释放载荷缓冲区和 Message 对象
 * @param[in] msg 要释放的 IMessage*，可为 nullptr
 * @return PCH_SUCCESS 或 PCH_NULLPTR
 */
ErrorCode MessageCenter::freeMessage(IMessage* msg)
{
	// 如果消息不为空，释放数据缓冲区并删除 Message 对象
	if (msg) {
		// 如果数据不为空
		if (msg->getData()) {
			// 释放数据缓冲区
			delete[] msg->getData();
		}			
		// 删除 Message 对象
		delete msg;
		// 返回 PCH_SUCCESS
		return PCH_SUCCESS;
	}
	// 返回 PCH_NULLPTR
	return PCH_NULLPTR;
}

/**
 * @brief 本地同步分发或远程同步发送；成功后释放 request
 * @param[in] target 目标对象名
 * @param[in,out] request 请求消息；成功时由此实现释放并置为 nullptr
 * @param[out] response 非空时接收响应；调用方负责 freeMessage
 * @param[in] dstNodeID 目标节点，0 表示本进程
 * @param[in] timeout 超时时间（毫秒），供远程路径使用
 * @return PCH_SUCCESS 或错误码
 */
pch::ErrorCode pch::MessageCenter::sendMessage(const char* target, IMessage*& request, IMessage** response, uint32_t dstNodeID , uint32_t timeout)
{	
	// 如果目标对象名为空，返回 PCH_PARAM_INVALID
	if (target == nullptr || strcmp(target, "") == 0) {
		// 写入日志
		WriteLog(LogLevel::Debug, "Target is nullptr or empty");
		return PCH_PARAM_INVALID;
	}
	// 如果请求消息为空，返回 PCH_PARAM_INVALID
	if (request == nullptr) {
		// 写入日志
		WriteLog(LogLevel::Debug, "Message is nullptr");
		return PCH_PARAM_INVALID;
	}
	// 如果对象管理器为空，返回 PCH_OBJMANAGER_NULLPTR
	if (nullptr == _objManager) {
		// 写入日志
		WriteLog(LogLevel::Debug, "g_objectManager is nullptr");
		return PCH_OBJMANAGER_NULLPTR;
    }

	// 如果目标节点 ID 不为 0，走 sendRemoteMessage
	if (0 != dstNodeID) {
		// 走 sendRemoteMessage
		return sendRemoteMessage(dstNodeID,target,request,response,timeout);
	}
	
	// 同步路径：当前调用线程直接进入目标 handleMessage；无论成功与否，此实现负责释放 request
	// 保持接口文档语义"同步发送的消息由消息中心释放"一致（目标缺失/分发失败也不会泄漏）
	// 分发期间持有 in-use 计数：并发 deleteObject 会被延迟，避免 UAF
	ObjectInfo* objInfo = _objManager->getObjInfoForUse(target);
	int ret = invokeHandler(objInfo, request, response);
	if (objInfo) {
		_objManager->releaseObject(objInfo);
	}
	freeMessage(request);
	request = nullptr;
	return ret;
}

/**
 * @brief 按顺序对组中名称依次同步 invokeHandler，成功后释放消息
 * @param[in] group 目标对象名数组
 * @param[in] count 数组长度
 * @param[in,out] message 广播消息；返回前由此实现释放并置为 nullptr
 * @param[in] msgPolicy 顺序和错误策略
 * @param[in] timeout 保留（当前实现未使用）
 * @return PCH_SUCCESS 或错误码
 */
ErrorCode MessageCenter::multicastLocalMessage(const char* group[], int count, IMessage* &message, uint32_t msgPolicy, uint32_t timeout)
{
	// 如果消息为空，返回 PCH_PARAM_INVALID
	if (message == nullptr)
		return PCH_PARAM_INVALID;
	// 参数校验：count 不能为负，count > 0 时 group 不能为空
	if (count < 0 || (count > 0 && group == nullptr)) {
		WriteLog(LogLevel::Debug, "multicastLocalMessage: invalid group/count");
		return PCH_PARAM_INVALID;
	}
	(void)timeout;

    // 如果对象管理器为空，返回 PCH_OBJMANAGER_NULLPTR
    if (nullptr == _objManager)
	{
		WriteLog(LogLevel::Error, "g_objectManager is nullptr");
		return PCH_OBJMANAGER_NULLPTR;
    }
	// 按策略确定是否逆序
	bool reverse = (msgPolicy & ReverseOrder);
	// 按策略确定是否出错即中断
	bool breakError = (msgPolicy & BreakOnError);
	// 初始化返回码
	ErrorCode ret = PCH_SUCCESS;
	if (!reverse) {
		for (size_t i = 0; i < static_cast<size_t>(count); i++) {
			IMessage* msg = nullptr;
			ObjectInfo* objInfo = _objManager->getObjInfoForUse(group[i]);
			ret = invokeHandler(objInfo, message, &msg);
			if (objInfo) {
				_objManager->releaseObject(objInfo);
			}
			if (msg) {
				freeMessage(msg);
			}
			if (breakError && ret != PCH_SUCCESS) {
				break;
			}
		}
	} else {
		for (int i = count - 1; i >= 0; i--) {
			IMessage* msg = nullptr;
			ObjectInfo* objInfo = _objManager->getObjInfoForUse(group[i]);
			ret = invokeHandler(objInfo, message, &msg);
			if (objInfo) {
				_objManager->releaseObject(objInfo);
			}
			if (msg) {
				freeMessage(msg);
			}
			if (breakError && ret != PCH_SUCCESS) {
				break;
			}
		}
	}

	// 无论是否中途 break，MessageCenter 回收消息体，避免 BreakOnError 路径泄漏
	freeMessage(message);
	message = nullptr;
	return ret;
}

/**
 * @brief dstNodeID==0 为本地异步广播，否则为远程异步广播
 * @param[in,out] message 广播消息；返回前由此实现释放并置为 nullptr
 * @param[in] dstNodeID 目标节点，0 表示本进程
 * @param[in] msgPolicy 顺序和错误策略
 * @return PCH_SUCCESS 或错误码
 */
ErrorCode MessageCenter::broadcastMessageAsync(IMessage*& message, uint32_t dstNodeID , uint32_t msgPolicy )
{
	// 本地异步广播，走 broadcastLocalMessageAsync
	if (0 == dstNodeID) {
		return broadcastLocalMessageAsync(message, msgPolicy);
	}
	// 远程异步广播，走 broadcastRemoteMessageAsync
	return broadcastRemoteMessageAsync(dstNodeID, message);
}

/**
 * @brief 遍历 _objManager 已注册对象进行同步分发，最后释放消息
 * @param[in,out] message 广播消息；返回前由此实现释放并置为 nullptr
 * @param[in] msgPolicy 顺序和错误策略
 * @param[in] timeout 保留（当前实现未使用）
 * @return PCH_SUCCESS 或错误码
 */
ErrorCode MessageCenter::broadcastLocalMessage(IMessage* &message, uint32_t msgPolicy, uint32_t timeout)
{
	(void)timeout;
	if (message == nullptr) {
		return PCH_PARAM_INVALID;
	}
	if (_objManager == nullptr) {
		WriteLog(LogLevel::Debug, "g_objectManager is nullptr!");
		freeMessage(message);
		message = nullptr;
		return PCH_OBJMANAGER_NULLPTR;
	}

	bool reverse = (msgPolicy & ReverseOrder);
	bool breakError = (msgPolicy & BreakOnError);
	ErrorCode ret = PCH_SUCCESS;
	auto objList = _objManager->getRegisterObjectsForUse();

	auto dispatch = [&](ObjectInfo* obj) {
		if (obj == nullptr) {
			return;
		}
		IMessage* resp = nullptr;
		ret = invokeHandler(obj, message, &resp);
		if (resp) {
			freeMessage(resp);
		}
	};

	if (!reverse) {
		for (const auto& obj : objList) {
			dispatch(obj);
			if (breakError && ret != PCH_SUCCESS && ret != PCH_MESSAGEHANDLER_NOTFOUND) {
				break;
			}
		}
	} else {
		for (auto it = objList.rbegin(); it != objList.rend(); ++it) {
			dispatch(*it);
			if (breakError && ret != PCH_SUCCESS && ret != PCH_MESSAGEHANDLER_NOTFOUND) {
				break;
			}
		}
	}

	// 遍历分发完成，释放全部 in-use 计数（期间被标记待删除的对象在此实际销毁）
	for (auto* oi : objList) {
		_objManager->releaseObject(oi);
	}

	// 确保所有路径释放消息，避免 BreakOnError 分支泄漏
	freeMessage(message);
	message = nullptr;
	// 原语义：分发完成返回 PCH_SUCCESS；中间错误视为可忽略（与调用方用法一致）
	return (ret == PCH_MESSAGEHANDLER_NOTFOUND || ret == PCH_OBJECT_NOTFOUND) ? PCH_SUCCESS : ret;
}

/**
 * @brief 本地入队单个目标或走 postRemoteMessage
 * @param[in] target 目标对象名
 * @param[in,out] message 要分发的消息；返回前由此实现释放并置为 nullptr
 * @param[in] dstNodeID 目标节点，0 表示本进程入队
 * @return PCH_SUCCESS 或错误码
 */
ErrorCode MessageCenter::postMessage(const char* target, IMessage*& message, uint32_t dstNodeID )
{
	// 如果目标对象名为空，释放消息然后返回错误码（与接口文档一致：异步消息由中心释放）
	if (target == nullptr || target[0] == '\0' || message == nullptr) {
		if (message) {
			freeMessage(message);
			message = nullptr;
		}
		return PCH_PARAM_INVALID;
	}

	// 如果对象管理器为空，释放消息避免泄漏
	if (nullptr == _objManager) {
		freeMessage(message);
		message = nullptr;
		return PCH_OBJMANAGER_NULLPTR;
	}

	// 分布式消息，走 postRemoteMessage（内部处理释放）
	if (0 != dstNodeID) {
		return postRemoteMessage(dstNodeID, target, message);
	}

	// 进程内异步：仅入队；MessageCenter::run 工作线程稍后按名称解析并 invokeHandler
	// 即使此处预先检查目标存在，入队后对象仍可能被销毁；真正校验留给工作线程，目标缺失时消息由工作线程释放
	if (_objManager->getObjInfo(target) == nullptr) {
		// 目标不存在：按照文档，此实现负责释放消息
		freeMessage(message);
		message = nullptr;
		return PCH_OBJECT_NOTFOUND;
	}
	enqueueMessage({ std::string(target) }, message);
	return PCH_SUCCESS;
}

/**
 * @brief 将组解析为 ObjectInfo* 列表并入队供异步分发
 * @param[in] group 目标对象名数组
 * @param[in] count 数组长度
 * @param[in,out] message 广播消息；返回前由此实现释放并置为 nullptr
 * @param[in] msgPolicy 本地路径的顺序策略位（见 ReverseOrder/BreakOnError）
 * @return PCH_SUCCESS 或错误码
 */
ErrorCode MessageCenter::multicastLocalMessageAsync(const char* group[], int count, IMessage* &message, uint32_t msgPolicy)
{
	if (message == nullptr) {
		return PCH_PARAM_INVALID;
	}
	// 参数校验：count 不能为负，count > 0 时 group 不能为空
	if (count < 0 || (count > 0 && group == nullptr)) {
		WriteLog(LogLevel::Debug, "multicastLocalMessageAsync: invalid group/count");
		return PCH_PARAM_INVALID;
	}

	if (nullptr == _objManager) {
		WriteLog(LogLevel::Debug, "g_objectManager is nullptr");
		freeMessage(message);
		message = nullptr;
		return PCH_OBJMANAGER_NULLPTR;
	}

	bool reverse = (msgPolicy & ReverseOrder);
	bool breakError = (msgPolicy & BreakOnError);

	ErrorCode ret = PCH_SUCCESS;
	std::vector<std::string> names;
	// 仅做存在性预检和名称收集；真正解析留给工作线程，避免持有原始 ObjectInfo*
	auto collect = [&](const char* name) -> bool {
		if (name == nullptr || name[0] == '\0') {
			return true;
		}
		if (_objManager->getObjInfo(name) != nullptr) {
			names.emplace_back(name);
			return true;
		}
		ret = PCH_OBJECT_NOTFOUND;
		return !breakError;
	};

	if (!reverse) {
		for (size_t i = 0; i < static_cast<size_t>(count); i++) {
			if (!collect(group[i])) {
				break;
			}
		}
	} else {
		for (int i = count - 1; i >= 0; i--) {
			if (!collect(group[i])) {
				break;
			}
		}
	}

	// BreakOnError 下，存在性失败时丢弃整个批次不入队并释放消息，避免部分分发
	if (breakError && ret != PCH_SUCCESS) {
		freeMessage(message);
		message = nullptr;
		return ret;
	}

	if (names.empty()) {
		// 无可分发目标，仍按异步语义释放消息
		freeMessage(message);
		message = nullptr;
		return ret;
	}

	enqueueMessage(std::move(names), message);
	return ret;
}

/**
 * @brief 将所有已注册对象入队（可按策略逆序）
 * @param[in,out] message 广播消息；返回前由此实现释放并置为 nullptr
 * @param[in] msgPolicy 本地路径的顺序策略位（见 ReverseOrder/BreakOnError）
 * @return PCH_SUCCESS 或错误码
 */
ErrorCode MessageCenter::broadcastLocalMessageAsync(IMessage* &message, uint32_t msgPolicy)
{
	if (message == nullptr) {
		return PCH_PARAM_INVALID;
	}

	if (_objManager == nullptr) {
		WriteLog(LogLevel::Debug, "ObjectManager is nullptr!");
		freeMessage(message);
		message = nullptr;
		return PCH_OBJMANAGER_NULLPTR;
	}

	bool reverse = (msgPolicy & ReverseOrder);

	// 快照已注册对象并取名称；异步广播仅覆盖命名对象（匿名对象无名称供工作线程解析）
	auto objLists = _objManager->getRegisterObjects();
	std::vector<std::string> names;
	names.reserve(objLists.size());
	for (auto* oi : objLists) {
		if (oi == nullptr || oi->objName.empty()) {
			continue;
		}
		names.push_back(oi->objName);
	}
	if (reverse) {
		std::reverse(names.begin(), names.end());
	}

	if (names.empty()) {
		freeMessage(message);
		message = nullptr;
		return PCH_SUCCESS;
	}

	enqueueMessage(std::move(names), message);
	return PCH_SUCCESS;
}

/**
 * @brief 工作线程：等待队列，将每个 MessagePack 分发给各目标并释放消息
 * @details 等待队列，将每个 MessagePack 分发给各目标并释放消息
 */
void MessageCenter::run()
{
	// 单一消费者线程：从 _messages 取出 MessagePack，按名称解析目标（避免裸 ObjectInfo* UAF），
	// 分发完成后统一 freeMessage
	while (!_isTerminate.load(std::memory_order_acquire)) {
		std::unique_lock<std::mutex> lk(_mutex);
		if (_messages.empty()) {
			_cv.wait(lk, [=]() {
				return !_messages.empty() || _isTerminate.load(std::memory_order_acquire);
			});
		}
		if (_messages.empty()) {
			continue;
		}
		MessagePack msgPack = std::move(_messages.front());
		_messages.pop_front();
		lk.unlock();

		for (const auto& name : msgPack._targetNames) {
			if (_objManager == nullptr) {
				break;
			}
			// 每次分发前按名称重新解析并持有 in-use 计数；如果目标已被删除则跳过
			ObjectInfo* target = _objManager->getObjInfoForUse(name.c_str());
			if (target == nullptr) {
				continue;
			}
			IMessage* resp = nullptr;
			try {
				invokeHandler(target, msgPack._msg, &resp);
			} catch (const std::exception& e) {
				WriteLog(LogLevel::Error, "async invokeHandler exception: %s", e.what());
				resp = nullptr;
			} catch (...) {
				WriteLog(LogLevel::Error, "async invokeHandler unknown exception");
				resp = nullptr;
			}
			_objManager->releaseObject(target);
			if (resp) {
				freeMessage(resp);
			}
		}

		if (msgPack._msg) {
			freeMessage(msgPack._msg);
			msgPack._msg = nullptr;
		}
	}
}

/**
 * @brief 从目标组件获取 IMessageHandler 并调用 handleMessage，可选填充响应或释放返回值
 * @param[in] objInfo 目标 ObjectInfo*
 * @param[in,out] req 请求消息；返回前由此实现释放并置为 nullptr
 * @param[out] response 非空时接收响应；调用方负责 freeMessage
 * @return PCH_SUCCESS、PCH_OBJECT_NOTFOUND、PCH_MESSAGEHANDLER_NOTFOUND 等
 */
ErrorCode MessageCenter::invokeHandler(ObjectInfo* objInfo, IMessage* &req, IMessage** response)
{
	// 如果对象管理器为空，返回错误码
	if (nullptr == _objManager) {
		WriteLog(LogLevel::Debug, "g_objectManager is nullptr");
		return PCH_OBJMANAGER_NULLPTR;
	}
	
	// 如果目标对象为空，返回错误码
	if (objInfo == nullptr) {
		return PCH_OBJECT_NOTFOUND;
	}

	// 如果目标对象组件、对象和请求消息都有效，调用 IMessageHandler::handleMessage
	if (objInfo->component && objInfo->object && req) {
		IMessageHandler* componentTmpl = (IMessageHandler*)objInfo->component->getComponentInfo()->getMessageHandler(objInfo->object);
		if (componentTmpl) {
			// 调用 IMessageHandler::handleMessage
			const IMessage* msg = componentTmpl->handleMessage(req);
			// 如果处理器错误地将 req 原样返回，忽略以避免与调用方双重释放
			if (msg == req) {
				msg = nullptr;
			}
			if (response) {
				*response = (IMessage*)msg;
			} else if (msg) {
				freeMessage((IMessage*)msg);
			}
			return PCH_SUCCESS;
		} 
		// 如果 IMessageHandler 为空，返回错误码
		else {
			// 返回 PCH_MESSAGEHANDLER_NOTFOUND
			return PCH_MESSAGEHANDLER_NOTFOUND;
		}
	} 
	// 如果目标对象为空，返回错误码
	else {
		// 如果目标对象为空，返回错误码
		std::string reason = "";
		if (objInfo == nullptr || objInfo->component == nullptr) {
			reason.append("[Target objectInfo is invalid]");
		}
		// 如果目标对象对象为空，返回错误码
		if (objInfo->object == nullptr) {
			reason.append("[Target has been deleted!]");
		}
		// 如果请求消息为空，返回错误码
		if (req == nullptr) {
			reason.append("[Message is nullptr]");
		}
		WriteLog(LogLevel::Debug, "The parameters to invoke 'onMessageCome' is invaild!, detail:%s", reason.c_str());
		return PCH_PARAM_INVALID;
	}
}

/**
 * @brief 组装 MessagePack，入队并 notify_one；调用方的 msg 置为 null
 * @param[in] targetNames 目标对象名列表（工作线程通过二次名称查找解析）
 * @param[in,out] msg 要入队的消息；输出在入队后置为 nullptr，所有权转移至队列
 */
void pch::MessageCenter::enqueueMessage(std::vector<std::string> targetNames, IMessage* &msg)
{
	MessagePack msgpack{ std::move(targetNames), msg };
	msg = nullptr;
	{
		std::unique_lock<std::mutex> lk(_mutex);
		_messages.emplace_back(std::move(msgpack));
	}
	_cv.notify_one();
}

/**
 * @brief 委托给 IMessageRelayer::sendMessage，然后释放 request
 * @param[in] dstNodeID 目标节点 ID
 * @param[in] target 目标对象名
 * @param[in,out] request 请求消息；返回前由此实现释放并置为 nullptr
 * @param[out] response 非空时接收响应；调用方负责 freeMessage
 * @param[in] timeout 超时时间（毫秒）
 * @return 中继器或本地检查结果
 */
ErrorCode pch::MessageCenter::sendRemoteMessage(uint32_t dstNodeID, const char* target, IMessage* &request, IMessage** response, uint32_t timeout)
{
	// 如果中继对象为空，返回错误码
	if (_messageRelayer == nullptr)
	{
		WriteLog(LogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(request);
		request = nullptr;
		return PCH_FAILED;
	}
	// 转发给 IMessageRelayer::sendMessage
	ErrorCode errCode = _messageRelayer->sendMessage(dstNodeID, target, request, response, timeout);
	// 释放请求
	freeMessage(request);
	// 设置请求为 nullptr
	request = nullptr;
	return errCode;
}

/**
 * @brief 委托给 IMessageRelayer::postMessage，然后释放消息
 * @param[in] dstNodeID 目标节点 ID
 * @param[in] target 目标对象名
 * @param[in,out] message 要分发的消息；返回前由此实现释放并置为 nullptr
 * @return 中继器或本地检查结果
 */
ErrorCode pch::MessageCenter::postRemoteMessage(uint32_t dstNodeID, const char* target, IMessage* &message)
{
	// 如果中继对象为空，返回错误码
	if (_messageRelayer == nullptr)
	{
		WriteLog(LogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(message);
		message = nullptr;
		return PCH_FAILED;
	}
	// 转发给 IMessageRelayer::postMessage
	ErrorCode errCode = _messageRelayer->postMessage(dstNodeID, target, message);
	// 释放消息
	freeMessage(message);
	// 设置消息为 nullptr
	message = nullptr;
	return errCode;
}

/**
 * @brief 本地或远程组播异步入口
 * @param[in] targets 目标对象名数组
 * @param[in] count 数组长度
 * @param[in,out] message 广播消息；返回前由此实现释放并置为 nullptr
 * @param[in] dstNodeID 目标节点 ID
 * @param[in] msgPolicy 本地路径的顺序策略位（见 ReverseOrder/BreakOnError）
 * @return PCH_SUCCESS 或错误码
 */
ErrorCode pch::MessageCenter::multicastMessageAsync(const char* targets[], int count, IMessage*& message, uint32_t dstNodeID , uint32_t msgPolicy)
{
	// 参数校验：count 不能为负，count > 0 时 targets 不能为空
	if (count < 0 || (count > 0 && targets == nullptr)) {
		WriteLog(LogLevel::Debug, "multicastMessageAsync: invalid targets/count");
		return PCH_PARAM_INVALID;
	}
	// 如果目标节点 ID 为 0，本地组播
	if (0 == dstNodeID) {
		return multicastLocalMessageAsync(targets, count, message, msgPolicy);
	}
	// 转发给 IMessageRelayer::multicastRemoteMessageAsync
	return multicastRemoteMessageAsync(dstNodeID, targets, count, message);
}

/**
 * @brief 委托给中继器组播异步，完成后释放消息
 * @param[in] dstNodeID 目标节点 ID
 * @param[in] targets 目标对象名数组
 * @param[in] count 数组长度
 * @param[in,out] message 广播消息；返回前由此实现释放并置为 nullptr
 * @return 中继器或本地检查结果
 */
ErrorCode pch::MessageCenter::multicastRemoteMessageAsync(uint32_t dstNodeID, const char* targets[], int count, IMessage* &message)
{
	// 如果中继对象为空，返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(LogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(message);
		message = nullptr;
		return PCH_FAILED;
	}
	// 转发给 IMessageRelayer::multicastRemoteMessageAsync
	ErrorCode errCode = _messageRelayer->multicastRemoteMessageAsync(dstNodeID, targets, count, message);
	// 释放消息
	freeMessage(message);
	// 设置消息为 nullptr
	message = nullptr;
	return errCode;
}

/**
 * @brief 委托给中继器广播异步，完成后释放消息
 * @param[in] dstNodeID 目标节点 ID
 * @param[in,out] message 广播消息；返回前由此实现释放并置为 nullptr
 * @return 中继器或本地检查结果
 */
ErrorCode pch::MessageCenter::broadcastRemoteMessageAsync(uint32_t dstNodeID, IMessage* &message)
{
	// 如果中继对象为空，返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(LogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(message);
		message = nullptr;
		return PCH_FAILED;
	}
	// 转发给 IMessageRelayer::broadcastRemoteMessageAsync
	ErrorCode errCode = _messageRelayer->broadcastRemoteMessageAsync(dstNodeID, message);
	// 释放消息
	freeMessage(message);
	// 设置消息为 nullptr
	message = nullptr;
	return errCode;
}

/**
 * @brief 转发给 IMessageRelayer::joinGroup
 * @param[in] groupId 组 ID
 * @return PCH_SUCCESS 或错误码
 */
ErrorCode MessageCenter::joinGroup(uint32_t groupId)
{
	// 如果中继对象为空，返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(LogLevel::Error, "_messageRelayer is nullptr");
		return PCH_FAILED;
	}
	// 转发给 IMessageRelayer::joinGroup
	return _messageRelayer->joinGroup(groupId);
}

/**
 * @brief 转发给 IMessageRelayer::leaveGroup
 * @param[in] groupId 组 ID
 * @return PCH_SUCCESS 或错误码
 */
ErrorCode MessageCenter::leaveGroup(uint32_t groupId)
{
	// 如果中继对象为空，返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(LogLevel::Error, "_messageRelayer is nullptr");
		return PCH_FAILED;
	}
	// 转发给 IMessageRelayer::leaveGroup
	return _messageRelayer->leaveGroup(groupId);
}

/**
 * @brief 转发给 IMessageRelayer::getGroupMembers
 * @param[in] groupId 组 ID
 * @return 组成员列表；无中继时返回空字符串
 */
const char* MessageCenter::getGroupMembers(uint32_t groupId)
{
	// 如果中继对象为空，返回空字符串
	if (_messageRelayer == nullptr) {
		WriteLog(LogLevel::Error, "_messageRelayer is nullptr");
		return "";
	}
	// 转发给 IMessageRelayer::getGroupMembers
	return _messageRelayer->getGroupMembers(groupId);	
}

/**
 * @brief 收到 SystemReady 时，从环境变量获取中继对象名并在 _objManager 中查找 _messageRelayer
 * @param[in] msg 系统消息
 * @return 成功返回 IMessage*；失败返回 nullptr
 */
const pch::IMessage* MessageCenter::handleMessage(const pch::IMessage* msg)
{
	// 如果消息为空，返回 nullptr
	if (msg == nullptr){
		return nullptr;
	}

	// 按消息类型处理
	switch (msg->getCode())
	{
		// 系统就绪消息
		case pch::SystemReady:
		{
			// 从环境变量获取中继对象名；如果 _environment 未初始化则使用默认名称，避免空指针解引用
			const char* name = nullptr;
			if (_environment != nullptr) {
				name = _environment->get("PCH_MESSAGE_RELAY");
			}
			if (name == nullptr || name[0] == '\0') {
				name = "PCH_MESSAGE_RELAY";
			}
			if (_objManager != nullptr) {
				_messageRelayer = (IMessageRelayer*)_objManager->findObject(name);
			}
		}
		break;
	}
	return nullptr; // 返回 nullptr
}

PCH_END_NAMESPACE
