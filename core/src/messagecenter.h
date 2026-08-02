/**
 * @file messagecenter.h
 * @brief 消息中心：分配/释放消息，点对点、组播、广播及后台分发
 * @details Message 是 IMessage 实现类；MessageCenter 是 IMessageCenter/IMessageHandler 实现。实现在 messagecenter.cpp 中。
 */
#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <thread>
#include <unordered_map>
#include "interface.h"
#include "objectManager.h"

PCH_BEGIN_NAMESPACE

/**
 * @class Message
 * @brief 默认 IMessage 实现：携带类型、业务码、载荷及发送者/接收者标识
 * @details 由 MessageCenter::allocMessage 等分配；由 freeMessage 回收。_data 由 center 按长度分配；调用方不得溢出。
 */
class Message : public IMessage
{
public:
	/** @brief 消息类型 */
	virtual uint32_t getType() const override
	{
		return _type;
	}

	/** @brief 业务码 */
	virtual uint32_t getCode() const override
	{
		return _code;
	}

	/** @brief 数据字节长度 */
	virtual uint32_t getSize() const override
	{
		return _dataLen;
	}

	/** @brief 数据缓冲区指针（长度 = getSize()） */
	virtual unsigned char* getData() const override
	{
		return _data;
	}

	/** @brief 发送者对象名 */
	virtual const char* getSource() const override
	{
		return _sender.c_str();
	}

	/** @brief 第 index 个接收者的名称；越界返回空字符串 */
	virtual const char* getTarget(uint16_t index) const override
	{
		if (index >= _targets.size())
		{
			return "";
		} 
		else 
		{
			return _targets.at(index).c_str();
		}
	}
	
	/** @brief 接收者数量 */
	virtual uint16_t getTargetsCount() const override
	{
		// 接口当前为 uint16_t；对于超长目标列表，安全截断并记录警告，避免静默截断为 0 等
		// 如果业务确实需要 >UINT16_MAX 个接收者，应以 ABI 兼容方式将接口升级为 uint32_t
		const size_t n = _targets.size();
		if (n > static_cast<size_t>(UINT16_MAX)) {
			return UINT16_MAX;
		}
		return static_cast<uint16_t>(n);
	}

	uint32_t                 _type = 0;              // 类型
	uint32_t                 _code;                  // 业务码
	uint32_t                 _dataLen;               // 数据长度
	std::string              _sender;                // 发送者
	std::vector<std::string> _targets;               // 接收者列表
	unsigned                 char* _data = nullptr;  // 数据
};

/**
 * @class MessageCenter
 * @brief 实现 IMessageCenter 和 IMessageHandler：消息分配/回收、同步/异步分发、组播和广播
 * @details 内置工作线程和队列；远程路径可通过 IMessageRelayer。实现在 messagecenter.cpp 中。
 */
class MessageCenter: public IMessageCenter, public IMessageHandler
{
public:
	/**
	 * @brief 构造并启动后台分发线程
	 */
	MessageCenter();

	/**
	 * @brief 设置退出标志、唤醒条件变量并 join 工作线程
	 */
	~MessageCenter();

	/**
	 * @brief 分配 Message；当 size > 0 时，分配零填充的载荷缓冲区
	 * @param[in] code 消息业务码
	 * @param[in] size 载荷字节数（可为 0）
	 * @param[in] sender 发送者对象名，可为 nullptr
	 * @return 成功返回 IMessage*；分配失败返回 nullptr
	 */
	virtual IMessage* allocMessage(uint32_t code, uint32_t size, const char* sender);

	/**
	 * @brief 分配 Message；当 data 非空时，拷贝 dataLen 字节到缓冲区
	 * @param[in] code 消息业务码
	 * @param[in] sender 发送者对象名，可为 nullptr
	 * @param[in] dataLen 载荷长度；按此长度分配缓冲区
	 * @param[in] data 源数据指针；为 nullptr 时只分配空间不拷贝
	 * @param[in] type 消息类型
	 * @return 成功返回 IMessage*；分配失败返回 nullptr
	 */
	virtual IMessage* allocMessage(uint32_t code, const char* sender, uint32_t dataLen, void* data = nullptr, uint32_t type = 0) override;

	/**
	 * @brief 释放消息对象及其载荷内存
	 * @param[in] msg 要释放的 IMessage*，可为 nullptr
	 * @return PCH_SUCCESS 或 PCH_NULLPTR
	 */
	virtual ErrorCode freeMessage(IMessage* msg) override;

	/**
	 * @brief 同步请求-响应；当 dstNodeID != 0 时，通过 sendRemoteMessage 走远程
	 * @param[in] target 目标对象名
	 * @param[in,out] request 请求消息；成功时由此实现释放并置为 nullptr
	 * @param[out] response 非空时接收响应；调用方负责 freeMessage
	 * @param[in] dstNodeID 目标节点，0 表示本进程
	 * @param[in] timeout 超时时间（毫秒），供远程路径使用
	 * @return PCH_SUCCESS 或错误码
	 * @note 同步分发使用目标对象的裸指针，调用方须保证分发期间目标对象不会被并发
	 *       deleteObject / tearDown；否则可能触发悬垂指针访问（与异步路径不同，
	 *       异步路径已按名称二次解析规避该问题）
	 */
	virtual ErrorCode sendMessage(const char* target, IMessage*& request, IMessage** response,  uint32_t dstNodeID = 0, uint32_t timeout = 0) override;

	/**
	 * @brief 异步分发到单个目标；当 dstNodeID != 0 时，通过 postRemoteMessage 走远程
	 * @param[in] target 目标对象名
	 * @param[in,out] message 要分发的消息；由消息中心释放，返回后不得复用
	 * @param[in] dstNodeID 目标节点，0 表示本进程入队
	 * @return PCH_SUCCESS 或错误码
	 */
	virtual ErrorCode postMessage(const char* target, IMessage*& message, uint32_t dstNodeID = 0) override;

	/**
	 * @brief 通过 IMessageRelayer 同步发送到远程节点
	 * @param[in] dstNodeID 目标节点 ID
	 * @param[in] target 目标对象名
	 * @param[in,out] request 请求消息；返回前由此实现释放并置为 nullptr
	 * @param[out] response 非空时接收响应；调用方负责 freeMessage
	 * @param[in] timeout 超时时间（毫秒）
	 * @return 中继器或本地检查结果
	 */
	virtual ErrorCode sendRemoteMessage(uint32_t dstNodeID, const char* target, IMessage* &request, IMessage** response, uint32_t timeout = 0);

	/**
	 * @brief 通过 IMessageRelayer 异步投递到远程节点
	 * @param[in] dstNodeID 目标节点 ID
	 * @param[in] target 目标对象名
	 * @param[in,out] message 要分发的消息；返回前由此实现释放并置为 nullptr
	 * @return 中继器或本地检查结果
	 */
	virtual ErrorCode postRemoteMessage(uint32_t dstNodeID, const char* target, IMessage* &message);

	/**
	 * @brief 组播异步：dstNodeID == 0 为本地入队，否则走 multicastRemoteMessageAsync
	 * @param[in] targets 目标对象名数组
	 * @param[in] count 数组长度
	 * @param[in,out] message 要分发的消息；由此实现释放，返回后不得复用
	 * @param[in] dstNodeID 目标节点，0 表示本进程
	 * @param[in] msgPolicy 本地路径的顺序策略位（见 ReverseOrder/BreakOnError）
	 * @return PCH_SUCCESS 或错误码
	 */
	virtual ErrorCode multicastMessageAsync(const char* targets[], int count, IMessage*& message, uint32_t dstNodeID = 0, uint32_t msgPolicy = DefaultMsgPolic) override;

	/**
	 * @brief 进程内按名称列表同步分发；顺序和错误行为由 msgPolicy 控制
	 * @param[in] group 目标对象名数组
	 * @param[in] count 数组长度
	 * @param[in,out] message 请求消息；全部成功时由此实现释放并置为 nullptr
	 * @param[in] msgPolicy 策略位
	 * @param[in] timeout 保留（当前实现未使用）
	 * @return PCH_SUCCESS 或错误码
	 */
	virtual ErrorCode multicastLocalMessage(const char* group[], int count, IMessage* &message, uint32_t msgPolicy = DefaultMsgPolic, uint32_t timeout = 0) override;

	/**
	 * @brief 进程内组播入队，由工作线程异步分发
	 * @param[in] group 目标对象名数组
	 * @param[in] count 数组长度
	 * @param[in,out] message 要分发的消息；入队后指针清空，由队列侧释放
	 * @param[in] msgPolicy 顺序策略（如 ReverseOrder）
	 * @return PCH_SUCCESS 或错误码（部分名称解析失败仍可能入队）
	 */
	virtual ErrorCode multicastLocalMessageAsync(const char* group[], int count, IMessage* &message, uint32_t msgPolicy = 0);


	/**
	 * @brief 广播异步：dstNodeID == 0 为本地 broadcastLocalMessageAsync，否则走远程
	 * @param[in,out] message 要广播的消息；由此实现释放
	 * @param[in] dstNodeID 目标节点，0 表示本进程
	 * @param[in] msgPolicy 本地路径策略
	 * @return PCH_SUCCESS 或错误码
	 */
	virtual ErrorCode broadcastMessageAsync(IMessage*& message, uint32_t dstNodeID = 0, uint32_t msgPolicy = DefaultMsgPolic) override;

	/**
	 * @brief 进程内同步分发到所有已注册对象，之后释放消息
	 * @param[in,out] message 广播消息；返回前释放并置为 nullptr
	 * @param[in] msgPolicy 顺序和错误策略
	 * @param[in] timeout 保留（当前实现未使用）
	 * @return PCH_SUCCESS 或错误码
	 * @note 同 sendMessage：同步分发期间目标对象不得被并发删除
	 */
	virtual ErrorCode broadcastLocalMessage(IMessage* &message, uint32_t msgPolicy = DefaultMsgPolic, uint32_t timeout = 0) override;

	/**
	 * @brief 进程内广播入队，由工作线程分发
	 * @param[in,out] message 广播消息；入队后指针清空
	 * @param[in] msgPolicy 顺序策略
	 * @return PCH_SUCCESS 或错误码
	 */
	virtual ErrorCode broadcastLocalMessageAsync(IMessage* &message, uint32_t msgPolicy = 0);

	/**
	 * @brief 远程组播异步，委托给 IMessageRelayer；返回前释放消息
	 * @param[in] dstNodeID 目标节点 ID
	 * @param[in] targets 目标对象名数组
	 * @param[in] count 数组长度
	 * @param[in,out] message 要分发的消息；返回前释放并置为 nullptr
	 * @return 中继器或本地检查结果
	 */
	virtual ErrorCode multicastRemoteMessageAsync(uint32_t dstNodeID, const char* targets[], int count, IMessage* &message) override;

	/**
	 * @brief 远程广播异步，委托给 IMessageRelayer；返回前释放消息
	 * @param[in] dstNodeID 目标节点 ID
	 * @param[in,out] message 要广播的消息；返回前释放并置为 nullptr
	 * @return 中继器或本地检查结果
	 */
	virtual ErrorCode broadcastRemoteMessageAsync(uint32_t dstNodeID, IMessage* &message) override;

	/**
	 * @brief 加入分布式组，委托给 IMessageRelayer::joinGroup
	 * @param[in] groupId 组 ID
	 * @return PCH_SUCCESS 或错误码
	 */
	virtual ErrorCode joinGroup(uint32_t groupId) override;

	/**
	 * @brief 离开组，委托给 IMessageRelayer::leaveGroup
	 * @param[in] groupId 组 ID
	 * @return PCH_SUCCESS 或错误码
	 */
	virtual ErrorCode leaveGroup(uint32_t groupId) override;

	/**
	 * @brief 查询组成员描述字符串；无中继时返回空字符串
	 * @param[in] groupId 组 ID，指明要查询哪个分布式组的成员
	 * @return 成员列表文本；生命周期按 Relayer 约定，请勿长期缓存
	 */
	virtual const char* getGroupMembers(uint32_t groupId) override;

	/**
	 * @brief 将目标对象解析为 IMessageHandler 并调用 handleMessage
	 * @param[in] objInfo 目标 ObjectInfo*，可为 nullptr
	 * @param[in,out] req 请求消息
	 * @param[out] response 非空时写入响应指针；否则此实现会 freeMessage 任何响应
	 * @return PCH_SUCCESS、PCH_OBJECT_NOTFOUND、PCH_MESSAGEHANDLER_NOTFOUND 等
	 */
	ErrorCode invokeHandler(ObjectInfo* objInfo, IMessage* &req, IMessage** response);

	/**
	 * @brief 系统消息回调（如 SystemReady 时，从环境变量解析中继对象并绑定 _messageRelayer）
	 * @param[in] msg 系统消息或其他消息
	 * @return 当前实现始终返回 nullptr
	 */
	virtual const pch::IMessage* handleMessage(const pch::IMessage* msg);
private:
	/**
	 * @brief 工作线程主循环：等待队列，对每个任务调用 invokeHandler 并释放消息
	 */
	void run();

	/**
	 * @brief 将异步分发任务入队并唤醒线程；调用后 msg 置为 nullptr，所有权转移至队列
	 * @param[in] targetNames 接收者对象名列表（异步路径按名称解析，避免持有 ObjectInfo* 被并发销毁导致的 UAF）
	 * @param[in,out] msg 要入队的消息；输出参数置为 null
	 * @note 匿名对象 objName 为空，会被过滤；异步广播不覆盖匿名对象
	 */
	void enqueueMessage(std::vector<std::string> targetNames, IMessage* &msg);

private:
	/** @brief 异步队列单元：一组目标名称 + 一条消息 */
	struct MessagePack
	{
		std::vector<std::string> _targetNames; // 接收者对象名列表（按名称解析，避免持有原始 ObjectInfo*）
		IMessage* _msg;                        // 要分发的消息（处理后释放）
	};
	std::atomic<bool>            _isTerminate;                // 请求线程退出（跨线程读写，用 atomic 避免数据竞争）
	std::deque<MessagePack>      _messages;                   // 异步任务队列
	std::mutex                   _mutex;                      // 保护 _messages 队列
	std::condition_variable      _cv;                         // 队列非空或退出时唤醒
	std::unique_ptr<std::thread> _thread;                     // 异步分发工作线程（入口 run）：构造时以 std::thread(&MessageCenter::run, this) 启动，析构时 join；与 _messages、_cv 协作消费本地异步入队的 post/multicast/broadcast 任务
	IMessageRelayer              *_messageRelayer = nullptr;  // 可选的分布式消息中继。初始为 nullptr；在 SystemReady 时，通过环境变量 PCH_MESSAGE_RELAY 使用 findObject 绑定（默认对象名与该字符串相同）。未注册或查找失败保持 null；此时 dstNodeID != 0 的远程路径及 join/leave group 等将失败。
};

PCH_END_NAMESPACE
