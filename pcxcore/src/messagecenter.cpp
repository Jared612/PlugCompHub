#include <string.h>
#include <algorithm>
#include "messagecenter.h"
#include "interface.h"
#include "pcxcomponent.h"
#include "pcxplugin.h"
#include <error.h>
#include "objectManager.h"
#include "loggerManager.h"
#include "environment.h"

PCX_REGISTER_COMPONENT(pcx::MessageCenter, PCX_MESSAGECENTER_ID)

PCX_BEGIN_NAMESPACE

extern ObjectManager* _objManager;  //全局对象管理器实例指针，由PCX初始化流程维护
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
 * @brief 请求后台投递线程退出并 join，并清空未派发的队列避免消息泄漏。
 * @details 置退出标志 → 唤醒工作线程 → join；随后把 `_messages` 中剩余的 `MessagePack`
 *          统一 `freeMessage` 回收，保证析构不会遗留未释放的消息缓冲。
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

	// 线程退出后没人会再访问 `_messages`，此处无需再加锁
	for (auto& pack : _messages) {
		if (pack._msg) {
			freeMessage(pack._msg);
			pack._msg = nullptr;
		}
	}
	_messages.clear();
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
	pcx::Message* msg = new pcx::Message();
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
IMessage* pcx::MessageCenter::allocMessage(uint32_t code, const char* sender, uint32_t dataLen, void* data, uint32_t type)
{
	// 创建 `Message` 对象
	pcx::Message* msg = new pcx::Message();
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
	if (dataLen > 0) {
		msg->_data = new (std::nothrow) unsigned char[dataLen];
		if (msg->_data == nullptr) {
			delete msg;
			return nullptr;
		}
		// 无论 data 是否为空，只要成功分配就登记长度，保证 getSize() 与缓冲区一致
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
 * @brief 释放负载缓冲区与 `Message` 对象。
 * @param[in] msg 待释放的 `IMessage*`，可为 nullptr
 * @return `PCX_SUCCESS` 或 `PCX_NULLPTR`
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
		// 返回 `PCX_SUCCESS`
		return PCX_SUCCESS;
	}
	// 返回 `PCX_NULLPTR`
	return PCX_NULLPTR;
}

/**
 * @brief 本地同步派发或远程同步发送；成功时释放 `request`。
 * @param[in] target 目标对象名
 * @param[in,out] request 请求消息；成功时由本实现释放并置 nullptr
 * @param[out] response 非空时接收应答；调用方负责 `freeMessage`
 * @param[in] dstNodeID 目标节点，0 表示本进程
 * @param[in] timeout 超时（毫秒），远程路径使用
 * @return `PCX_SUCCESS` 或错误码
 */
pcx::ErrorCode pcx::MessageCenter::sendMessage(const char* target, IMessage*& request, IMessage** response, uint32_t dstNodeID , uint32_t timeout)
{	
	// 如果目标对象名为空，则返回 `PCX_PARAM_INVALID`
	if (target == nullptr || strcmp(target, "") == 0) {
		// 写入日志
		WriteLog(PcxLogLevel::Debug, "Target is nullptr or empty");
		return PCX_PARAM_INVALID;
	}
	// 如果请求消息为空，则返回 `PCX_PARAM_INVALID`
	if (request == nullptr) {
		// 写入日志
		WriteLog(PcxLogLevel::Debug, "Message is nullptr");
		return PCX_PARAM_INVALID;
	}
	// 如果对象管理器为空，则返回 `PCX_OBJMANAGER_NULLPTR`
	if (nullptr == _objManager) {
		// 写入日志
		WriteLog(PcxLogLevel::Debug, "g_objectManager is nullptr");
		return PCX_OBJMANAGER_NULLPTR;
    }

	// 如果目标节点 ID 不为 0，则走 `sendRemoteMessage`
	if (0 != dstNodeID) {
		// 走 `sendRemoteMessage`
		return sendRemoteMessage(dstNodeID,target,request,response,timeout);
	}
	
	// 同步路径：当前调用线程直接进入目标 handleMessage；不论成功与否都由本实现负责释放 request，
	// 保持接口文档"同步发送的消息由消息中心释放"的语义一致（目标缺失/派发失败亦不泄漏）。
	int ret = invokeHandler(_objManager->getObjInfo(target), request, response);
	freeMessage(request);
	request = nullptr;
	return ret;
}

/**
 * @brief 对 `group` 内名字依次同步 `invokeHandler`，成功则释放 `message`。
 * @param[in] group 目标对象名数组
 * @param[in] count 数组长度
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @param[in] msgPolicy 顺序与遇错策略
 * @param[in] timeout 预留（当前实现未使用）
 * @return `PCX_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::multicastLocalMessage(const char* group[], int count, IMessage* &message, uint32_t msgPolicy, uint32_t timeout)
{
	// 如果消息为空，则返回 `PCX_PARAM_INVALID`
	if (message == nullptr)
		return PCX_PARAM_INVALID;

    // 如果对象管理器为空，则返回 `PCX_OBJMANAGER_NULLPTR`
    if (nullptr == _objManager)
	{
		WriteLog(PcxLogLevel::Error, "g_objectManager is nullptr");
		return PCX_OBJMANAGER_NULLPTR;
    }
	// 根据策略决定是否逆序
	bool reverse = (msgPolicy & ReverseOrder);
	// 根据策略决定是否遇到错误就停止
	bool breakError = (msgPolicy & BreakOnError);
	// 初始化返回码
	ErrorCode ret = PCX_SUCCESS;
	bool stopped = false;
	if (!reverse) {
		for (size_t i = 0; i < static_cast<size_t>(count); i++) {
			IMessage* msg = nullptr;
			ret = invokeHandler(_objManager->getObjInfo(group[i]), message, &msg);
			if (msg) {
				freeMessage(msg);
			}
			if (breakError && ret != PCX_SUCCESS) {
				stopped = true;
				break;
			}
		}
	} else {
		for (int i = count - 1; i >= 0; i--) {
			IMessage* msg = nullptr;
			ret = invokeHandler(_objManager->getObjInfo(group[i]), message, &msg);
			if (msg) {
				freeMessage(msg);
			}
			if (breakError && ret != PCX_SUCCESS) {
				stopped = true;
				break;
			}
		}
	}

	// 无论是否中途 break，都由 MessageCenter 回收消息本体，避免 BreakOnError 路径泄漏
	freeMessage(message);
	message = nullptr;
	(void)stopped;
	return ret;
}

/**
 * @brief `dstNodeID==0` 为本地异步广播，否则远程异步广播。
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @param[in] dstNodeID 目标节点，0 表示本进程
 * @param[in] msgPolicy 顺序与遇错策略
 * @return `PCX_SUCCESS` 或错误码
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
 * @return `PCX_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::broadcastLocalMessage(IMessage* &message, uint32_t msgPolicy, uint32_t timeout)
{
	if (message == nullptr) {
		return PCX_PARAM_INVALID;
	}
	if (_objManager == nullptr) {
		WriteLog(PcxLogLevel::Debug, "g_objectManager is nullptr!");
		freeMessage(message);
		message = nullptr;
		return PCX_OBJMANAGER_NULLPTR;
	}

	bool reverse = (msgPolicy & ReverseOrder);
	bool breakError = (msgPolicy & BreakOnError);
	ErrorCode ret = PCX_SUCCESS;
	auto objList = _objManager->getRegisterObjects();

	auto dispatch = [&](ObjectInfo* obj) {
		IMessage* resp = nullptr;
		ret = invokeHandler(obj, message, &resp);
		if (resp) {
			freeMessage(resp);
		}
	};

	if (!reverse) {
		for (const auto& obj : objList) {
			dispatch(obj);
			if (breakError && ret != PCX_SUCCESS && ret != PCX_MESSAGEHANDLER_NOTFOUND) {
				break;
			}
		}
	} else {
		for (auto it = objList.rbegin(); it != objList.rend(); ++it) {
			dispatch(*it);
			if (breakError && ret != PCX_SUCCESS && ret != PCX_MESSAGEHANDLER_NOTFOUND) {
				break;
			}
		}
	}

	// 确保无论哪条路径都释放消息，避免 BreakOnError 分支泄漏
	freeMessage(message);
	message = nullptr;
	// 原语义：派发完成即返回 PCX_SUCCESS；中间错误被视为可忽略（与调用方使用习惯一致）
	return (ret == PCX_MESSAGEHANDLER_NOTFOUND || ret == PCX_OBJECT_NOTFOUND) ? PCX_SUCCESS : ret;
}

/**
 * @brief 本地入队单目标或 `postRemoteMessage`。
 * @param[in] target 目标对象名
 * @param[in,out] message 待投递消息；返回前由本实现释放并置 nullptr
 * @param[in] dstNodeID 目标节点，0 表示本进程入队
 * @return `PCX_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::postMessage(const char* target, IMessage*& message, uint32_t dstNodeID )
{
	// 如果目标对象名为空，则释放消息后返回错误码（与接口文档"异步消息由中心释放"的约定一致）
	if (target == nullptr || target[0] == '\0' || message == nullptr) {
		if (message) {
			freeMessage(message);
			message = nullptr;
		}
		return PCX_PARAM_INVALID;
	}

	// 如果对象管理器为空，释放消息避免泄漏
	if (nullptr == _objManager) {
		freeMessage(message);
		message = nullptr;
		return PCX_OBJMANAGER_NULLPTR;
	}

	// 分布式消息，走 `postRemoteMessage`（内部负责释放）
	if (0 != dstNodeID) {
		return postRemoteMessage(dstNodeID, target, message);
	}

	// 本进程异步：仅入队，由 MessageCenter::run 工作线程按名再解析并 invokeHandler。
	// 即使这里预检到目标存在，对象也可能在入队后被销毁；真正的校验留给 worker 端，目标缺失时消息由 worker 释放。
	if (_objManager->getObjInfo(target) == nullptr) {
		// 目标不存在：按文档由本实现负责释放消息
		freeMessage(message);
		message = nullptr;
		return PCX_OBJECT_NOTFOUND;
	}
	enqueueMessage({ std::string(target) }, message);
	return PCX_SUCCESS;
}

/**
 * @brief 解析 `group` 为 `ObjectInfo*` 列表并入队异步投递。
 * @param[in] group 目标对象名数组
 * @param[in] count 数组长度
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @param[in] msgPolicy 本地路径下的顺序等策略位（见 `ReverseOrder`/`BreakOnError`）
 * @return `PCX_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::multicastLocalMessageAsync(const char* group[], int count, IMessage* &message, uint32_t msgPolicy)
{
	if (message == nullptr) {
		return PCX_PARAM_INVALID;
	}

	if (nullptr == _objManager) {
		WriteLog(PcxLogLevel::Debug, "g_objectManager is nullptr");
		freeMessage(message);
		message = nullptr;
		return PCX_OBJMANAGER_NULLPTR;
	}

	bool reverse = (msgPolicy & ReverseOrder);
	bool breakError = (msgPolicy & BreakOnError);

	ErrorCode ret = PCX_SUCCESS;
	std::vector<std::string> names;
	// 仅做存在性预检并收集名字；真正解析留给 worker 线程，避免持有裸 ObjectInfo*。
	auto collect = [&](const char* name) -> bool {
		if (name == nullptr || name[0] == '\0') {
			return true;
		}
		if (_objManager->getObjInfo(name) != nullptr) {
			names.emplace_back(name);
			return true;
		}
		ret = PCX_OBJECT_NOTFOUND;
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

	// BreakOnError 下若存在失败，放弃整批入队并释放消息，避免半吊子投递
	if (breakError && ret != PCX_SUCCESS) {
		freeMessage(message);
		message = nullptr;
		return ret;
	}

	if (names.empty()) {
		// 没有可投递的目标，按异步语义依旧释放消息
		freeMessage(message);
		message = nullptr;
		return ret;
	}

	enqueueMessage(std::move(names), message);
	return ret;
}

/**
 * @brief 将全部已注册对象（可按策略逆序）入队。
 * @param[in,out] message 待广播消息；返回前由本实现释放并置 nullptr
 * @param[in] msgPolicy 本地路径下的顺序等策略位（见 `ReverseOrder`/`BreakOnError`）
 * @return `PCX_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::broadcastLocalMessageAsync(IMessage* &message, uint32_t msgPolicy)
{
	if (message == nullptr) {
		return PCX_PARAM_INVALID;
	}

	if (_objManager == nullptr) {
		WriteLog(PcxLogLevel::Debug, "ObjectManager is nullptr!");
		freeMessage(message);
		message = nullptr;
		return PCX_OBJMANAGER_NULLPTR;
	}

	bool reverse = (msgPolicy & ReverseOrder);

	// 快照已注册对象并取其名字；异步广播仅覆盖具名对象（匿名对象没有名字可供 worker 再解析）
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
		return PCX_SUCCESS;
	}

	enqueueMessage(std::move(names), message);
	return PCX_SUCCESS;
}

/**
 * @brief 工作线程：等待队列、对每条 `MessagePack` 逐个目标派发并释放消息。
 * @details 等待队列、对每条 `MessagePack` 逐个目标派发并释放消息。
 */
void MessageCenter::run()
{
	// 单消费者线程：从 _messages 取 MessagePack，按名字再解析目标（避免裸 ObjectInfo* UAF），
	// 派发完成后统一 freeMessage。
	while (!_isTerminate) {
		std::unique_lock<std::mutex> lk(_mutex);
		if (_messages.empty()) {
			_cv.wait(lk, [=]() { return !_messages.empty() || _isTerminate; });
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
			// 每次派发前都重新按名解析，目标若已被删除则跳过；与 sync 路径 getObjInfo 走同一把锁
			ObjectInfo* target = _objManager->getObjInfo(name.c_str());
			if (target == nullptr) {
				continue;
			}
			IMessage* resp = nullptr;
			try {
				invokeHandler(target, msgPack._msg, &resp);
			} catch (const std::exception& e) {
				WriteLog(PcxLogLevel::Error, "async invokeHandler exception: %s", e.what());
				resp = nullptr;
			} catch (...) {
				WriteLog(PcxLogLevel::Error, "async invokeHandler unknown exception");
				resp = nullptr;
			}
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
 * @brief 从目标组件取 `IMessageHandler` 并调用 `handleMessage`，按需回填 `response` 或释放返回值。
 * @param[in] objInfo 目标 `ObjectInfo*`
 * @param[in,out] req 请求消息；返回前由本实现释放并置 nullptr
 * @param[out] response 非空时接收应答；调用方负责 `freeMessage`
 * @return `PCX_SUCCESS`、`PCX_OBJECT_NOTFOUND`、`PCX_MESSAGEHANDLER_NOTFOUND` 等
 */
ErrorCode MessageCenter::invokeHandler(ObjectInfo* objInfo, IMessage* &req, IMessage** response)
{
	// 如果对象管理器为空，则返回错误码
	if (nullptr == _objManager) {
		WriteLog(PcxLogLevel::Debug, "g_objectManager is nullptr");
		return PCX_OBJMANAGER_NULLPTR;
	}
	
	// 如果目标对象为空，则返回错误码
	if (objInfo == nullptr) {
		return PCX_OBJECT_NOTFOUND;
	}

	// 如果目标对象组件、对象、请求消息都有效，则调用 `IMessageHandler::handleMessage`
	if (objInfo->component && objInfo->object && req) {
		IMessageHandler* componentTmpl = (IMessageHandler*)objInfo->component->getComponentInfo()->getMessageHandler(objInfo->object);
		if (componentTmpl) {
			// 调用 `IMessageHandler::handleMessage`
			const IMessage* msg = componentTmpl->handleMessage(req);
			// 若 handler 误把 req 原样返回，忽略它，避免与调用方形成双重释放
			if (msg == req) {
				msg = nullptr;
			}
			if (response) {
				*response = (IMessage*)msg;
			} else if (msg) {
				freeMessage((IMessage*)msg);
			}
			return PCX_SUCCESS;
		} 
		// 如果 `IMessageHandler` 为空，则返回错误码
		else {
			// 返回 `PCX_MESSAGEHANDLER_NOTFOUND`
			return PCX_MESSAGEHANDLER_NOTFOUND;
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
		WriteLog(PcxLogLevel::Debug, "The parameters to invoke 'onMessageCome' is invaild!, detail:%s", reason.c_str());
		return PCX_PARAM_INVALID;
	}
}

/**
 * @brief 组装 `MessagePack` 入队并 `notify_one`，调用方 `msg` 置空。
 * @param[in] targetNames 目标对象名列表（worker 线程解析时按名二次查找）
 * @param[in,out] msg 待入队消息；入队后出参置 nullptr，所有权转移给队列
 */
void pcx::MessageCenter::enqueueMessage(std::vector<std::string> targetNames, IMessage* &msg)
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
 * @brief 委托 `IMessageRelayer::sendMessage`，随后释放 `request`。
 * @param[in] dstNodeID 目标节点 ID
 * @param[in] target 目标对象名
 * @param[in,out] request 请求消息；返回前由本实现释放并置 nullptr
 * @param[out] response 非空时接收应答；调用方负责 `freeMessage`
 * @param[in] timeout 超时（毫秒）
 * @return Relayer 或本层检查结果
 */
ErrorCode pcx::MessageCenter::sendRemoteMessage(uint32_t dstNodeID, const char* target, IMessage* &request, IMessage** response, uint32_t timeout)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr)
	{
		WriteLog(PcxLogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(request);
		request = nullptr;
		return PCX_FAILED;
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
ErrorCode pcx::MessageCenter::postRemoteMessage(uint32_t dstNodeID, const char* target, IMessage* &message)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr)
	{
		WriteLog(PcxLogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(message);
		message = nullptr;
		return PCX_FAILED;
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
 * @return `PCX_SUCCESS` 或错误码
 */
ErrorCode pcx::MessageCenter::multicastMessageAsync(const char* targets[], int count, IMessage*& message, uint32_t dstNodeID , uint32_t msgPolicy)
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
ErrorCode pcx::MessageCenter::multicastRemoteMessageAsync(uint32_t dstNodeID, const char* targets[], int count, IMessage* &message)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(PcxLogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(message);
		message = nullptr;
		return PCX_FAILED;
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
ErrorCode pcx::MessageCenter::broadcastRemoteMessageAsync(uint32_t dstNodeID, IMessage* &message)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(PcxLogLevel::Error, "_messageRelayer is nullptr");
		freeMessage(message);
		message = nullptr;
		return PCX_FAILED;
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
 * @return `PCX_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::joinGroup(uint32_t groupId)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(PcxLogLevel::Error, "_messageRelayer is nullptr");
		return PCX_FAILED;
	}
	// 转发 `IMessageRelayer::joinGroup`
	return _messageRelayer->joinGroup(groupId);
}

/**
 * @brief 转发 `IMessageRelayer::leaveGroup`。
 * @param[in] groupId 组 ID
 * @return `PCX_SUCCESS` 或错误码
 */
ErrorCode MessageCenter::leaveGroup(uint32_t groupId)
{
	// 如果中继对象为空，则返回错误码
	if (_messageRelayer == nullptr) {
		WriteLog(PcxLogLevel::Error, "_messageRelayer is nullptr");
		return PCX_FAILED;
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
		WriteLog(PcxLogLevel::Error, "_messageRelayer is nullptr");
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
const pcx::IMessage* MessageCenter::handleMessage(const pcx::IMessage* msg)
{
	// 如果消息为空，则返回 nullptr
	if (msg == nullptr){
		return nullptr;
	}

	// 根据消息类型进行处理
	switch (msg->getCode())
	{
		// 系统就绪消息
		case pcx::SystemReady:
		{
			// 从环境变量中获取中继对象名；`_environment` 未初始化时使用默认名，避免空解引用
			const char* name = nullptr;
			if (_environment != nullptr) {
				name = _environment->get("PCX_MESSAGE_RELAY");
			}
			if (name == nullptr || name[0] == '\0') {
				name = "PCX_MESSAGE_RELAY";
			}
			if (_objManager != nullptr) {
				_messageRelayer = (IMessageRelayer*)_objManager->findObject(name);
			}
		}
		break;
	}
	return nullptr; // 返回 nullptr
}

PCX_END_NAMESPACE