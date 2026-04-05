/**
 * @file messagecenter.h
 * @brief 消息中心：分配/释放消息，点对点、组播、广播及后台投递。
 * @details `Message` 为 `IMessage`的实现类；`MessageCenter` 为 `IMessageCenter`/`IMessageHandler`的实现类。实现见 `messagecenter.cpp`。
 */
#pragma once
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

MCP_BEGIN_NAMESPACE

/**
 * @class Message
 * @brief `IMessage` 的默认实现：承载类型、业务码、负载与收发端标识。
 * @details 由 `MessageCenter::allocMessage` 等分配；`freeMessage` 回收。`_data` 由中心按长度分配，调用方勿越界。
 */
class Message : public IMessage
{
public:
	/** @brief 消息类型。 */
	virtual uint32_t getType() const override
	{
		return _type;
	}

	/** @brief 业务码。 */
	virtual uint32_t getCode() const override
	{
		return _code;
	}

	/** @brief 数据字节长度。 */
	virtual uint32_t getSize() const override
	{
		return _dataLen;
	}

	/** @brief 数据缓冲区指针（长度为 getSize()）。 */
	virtual unsigned char* const getData() const override
	{
		return _data;
	}

	/** @brief 发送端对象名。 */
	virtual const char* getSource() const override
	{
		return _sender.c_str();
	}

	/** @brief 第 index 个接收者名称；越界返回空串。 */
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
	
	/** @brief 接收端个数。 */
	virtual const uint16_t getTargetsCount() const override
	{
		return _targets.size();
	}

	uint32_t                 _type = 0;              //类型
	uint32_t                 _code;                  //业务码
	uint32_t                 _dataLen;               //数据长度
	std::string              _sender;                //发送端
	std::vector<std::string> _targets;               //接收者列表
	unsigned                 char* _data = nullptr;  //数据
};

/**
 * @class MessageCenter
 * @brief 实现 `IMessageCenter` 与 `IMessageHandler`：消息分配回收、同步/异步投递、组播与广播。
 * @details 内置工作线程与队列；远程路径可走 `IMessageRelayer`。实现见 `messagecenter.cpp`。
 */
class MessageCenter: public IMessageCenter, public IMessageHandler
{
public:
	/**
	 * @brief 构造并启动后台投递线程。
	 */
	MessageCenter();

	/**
	 * @brief 置退出标志、唤醒条件变量并 join 工作线程。
	 */
	~MessageCenter();

	/**
	 * @brief 分配 `Message`；`size > 0` 时分配零填充负载缓冲区。
	 * @param[in] code 消息业务码
	 * @param[in] size 负载字节数（可为 0）
	 * @param[in] sender 发送端对象名，可为 nullptr
	 * @return 成功返回 `IMessage*`；分配失败返回 nullptr
	 */
	virtual IMessage* allocMessage(uint32_t code, uint32_t size, const char* sender);

	/**
	 * @brief 分配 `Message`；`data` 非空时将 `dataLen` 字节拷入缓冲区。
	 * @param[in] code 消息业务码
	 * @param[in] sender 发送端对象名，可为 nullptr
	 * @param[in] dataLen 负载长度；缓冲区按此长度分配
	 * @param[in] data 源数据指针；为 nullptr 时仅分配空间不拷贝
	 * @param[in] type 消息类型
	 * @return 成功返回 `IMessage*`；分配失败返回 nullptr
	 */
	virtual IMessage* allocMessage(uint32_t code, const char* sender, uint32_t dataLen, void* data = nullptr, uint32_t type = 0) override;

	/**
	 * @brief 释放消息对象及其负载内存。
	 * @param[in] msg 待释放的 `IMessage*`，可为 nullptr
	 * @return `MCP_SUCCESS` 或 `MCP_NULLPTR`
	 */
	virtual ErrorCode freeMessage(IMessage* msg) override;

	/**
	 * @brief 同步请求-应答；`dstNodeID != 0` 时走远程 `sendRemoteMessage`。
	 * @param[in] target 目标对象名
	 * @param[in,out] request 请求消息；成功时由本实现释放并置 nullptr
	 * @param[out] response 非空时接收应答；调用方负责 `freeMessage`
	 * @param[in] dstNodeID 目标节点，0 表示本进程
	 * @param[in] timeout 超时（毫秒），远程路径使用
	 * @return `MCP_SUCCESS` 或错误码
	 */
	virtual ErrorCode sendMessage(const char* target, IMessage*& request, IMessage** response,  uint32_t dstNodeID = 0, uint32_t timeout = 0) override;

	/**
	 * @brief 异步投递到单目标；`dstNodeID != 0` 时走 `postRemoteMessage`。
	 * @param[in] target 目标对象名
	 * @param[in,out] message 待投递消息；由消息中心释放，返回后勿再用
	 * @param[in] dstNodeID 目标节点，0 表示本进程入队
	 * @return `MCP_SUCCESS` 或错误码
	 */
	virtual ErrorCode postMessage(const char* target, IMessage*& message, uint32_t dstNodeID = 0) override;

	/**
	 * @brief 经 `IMessageRelayer` 同步发送到远程节点。
	 * @param[in] dstNodeID 目标节点 ID
	 * @param[in] target 目标对象名
	 * @param[in,out] request 请求消息；返回前由本实现释放并置 nullptr
	 * @param[out] response 非空时接收应答；调用方负责 `freeMessage`
	 * @param[in] timeout 超时（毫秒）
	 * @return Relayer 或本层检查结果
	 */
	virtual ErrorCode sendRemoteMessage(uint32_t dstNodeID, const char* target, IMessage* &request, IMessage** response, uint32_t timeout = 0);

	/**
	 * @brief 经 `IMessageRelayer` 异步 post 到远程节点。
	 * @param[in] dstNodeID 目标节点 ID
	 * @param[in] target 目标对象名
	 * @param[in,out] message 待投递消息；返回前由本实现释放并置 nullptr
	 * @return Relayer 或本层检查结果
	 */
	virtual ErrorCode postRemoteMessage(uint32_t dstNodeID, const char* target, IMessage* &message);

	/**
	 * @brief 组播异步：`dstNodeID == 0` 为本地入队，否则 `multicastRemoteMessageAsync`。
	 * @param[in] targets 目标对象名数组
	 * @param[in] count 数组长度
	 * @param[in,out] message 待投递消息；由本实现释放，返回后勿再用
	 * @param[in] dstNodeID 目标节点，0 表示本进程
	 * @param[in] msgPolicy 本地路径下的顺序等策略位（见 `ReverseOrder`/`BreakOnError`）
	 * @return `MCP_SUCCESS` 或错误码
	 */
	virtual ErrorCode multicastMessageAsync(const char* targets[], int count, IMessage*& message, uint32_t dstNodeID = 0, uint32_t msgPolicy = DefaultMsgPolic) override;

	/**
	 * @brief 本进程内按名列表同步派发；顺序与遇错行为由 `msgPolicy` 控制。
	 * @param[in] group 目标对象名数组
	 * @param[in] count 数组长度
	 * @param[in,out] message 请求消息；全部成功时由本实现释放并置 nullptr
	 * @param[in] msgPolicy 策略位
	 * @param[in] timeout 预留（当前实现未使用）
	 * @return `MCP_SUCCESS` 或错误码
	 */
	virtual ErrorCode multicastLocalMessage(const char* group[], int count, IMessage* &message, uint32_t msgPolicy = DefaultMsgPolic, uint32_t timeout = 0) override;

	/**
	 * @brief 本进程内组播入队，由工作线程异步投递。
	 * @param[in] group 目标对象名数组
	 * @param[in] count 数组长度
	 * @param[in,out] message 待投递消息；入队后指针被清空，由队列侧释放
	 * @param[in] msgPolicy 顺序策略（如 `ReverseOrder`）
	 * @return `MCP_SUCCESS` 或错误码（如部分名解析失败仍可能入队）
	 */
	virtual ErrorCode multicastLocalMessageAsync(const char* group[], int count, IMessage* &message, uint32_t msgPolicy = 0);


	/**
	 * @brief 广播异步：`dstNodeID == 0` 为本地 `broadcastLocalMessageAsync`，否则远程。
	 * @param[in,out] message 待广播消息；由本实现释放
	 * @param[in] dstNodeID 目标节点，0 表示本进程
	 * @param[in] msgPolicy 本地路径策略
	 * @return `MCP_SUCCESS` 或错误码
	 */
	virtual ErrorCode broadcastMessageAsync(IMessage*& message, uint32_t dstNodeID = 0, uint32_t msgPolicy = DefaultMsgPolic) override;

	/**
	 * @brief 本进程内向所有已注册对象同步派发，结束后释放 `message`。
	 * @param[in,out] message 广播消息；返回前释放并置 nullptr
	 * @param[in] msgPolicy 顺序与遇错策略
	 * @param[in] timeout 预留（当前实现未使用）
	 * @return `MCP_SUCCESS` 或错误码
	 */
	virtual ErrorCode broadcastLocalMessage(IMessage* &message, uint32_t msgPolicy = DefaultMsgPolic, uint32_t timeout = 0) override;

	/**
	 * @brief 本进程内广播入队，由工作线程投递。
	 * @param[in,out] message 待广播消息；入队后指针被清空
	 * @param[in] msgPolicy 顺序策略
	 * @return `MCP_SUCCESS` 或错误码
	 */
	virtual ErrorCode broadcastLocalMessageAsync(IMessage* &message, uint32_t msgPolicy = 0);

	/**
	 * @brief 远程组播异步，委托 `IMessageRelayer`；返回前释放 `message`。
	 * @param[in] dstNodeID 目标节点 ID
	 * @param[in] targets 目标对象名数组
	 * @param[in] count 数组长度
	 * @param[in,out] message 待投递消息；返回前释放并置 nullptr
	 * @return Relayer 或本层检查结果
	 */
	virtual ErrorCode multicastRemoteMessageAsync(uint32_t dstNodeID, const char* targets[], int count, IMessage* &message) override;

	/**
	 * @brief 远程广播异步，委托 `IMessageRelayer`；返回前释放 `message`。
	 * @param[in] dstNodeID 目标节点 ID
	 * @param[in,out] message 待广播消息；返回前释放并置 nullptr
	 * @return Relayer 或本层检查结果
	 */
	virtual ErrorCode broadcastRemoteMessageAsync(uint32_t dstNodeID, IMessage* &message) override;

	/**
	 * @brief 加入分布式组，委托 `IMessageRelayer::joinGroup`。
	 * @param[in] groupId 组 ID
	 * @return `MCP_SUCCESS` 或错误码
	 */
	virtual ErrorCode joinGroup(uint32_t groupId) override;

	/**
	 * @brief 离开组，委托 `IMessageRelayer::leaveGroup`。
	 * @param[in] groupId 组 ID
	 * @return `MCP_SUCCESS` 或错误码
	 */
	virtual ErrorCode leaveGroup(uint32_t groupId) override;

	/**
	 * @brief 查询组成员描述字符串；无中继时返回空串。
	 * @param[in] groupId 组 ID，表示你想查的是 哪一个分布式组 的成员。
	 * @return 成员列表等文本；生命周期由 Relayer 约定，勿长期缓存
	 */
	virtual const char* getGroupMembers(uint32_t groupId) override;

	/**
	 * @brief 对目标对象解析 `IMessageHandler` 并调用 `handleMessage`。
	 * @param[in] objInfo 目标 `ObjectInfo*`，可为 nullptr
	 * @param[in,out] req 请求消息
	 * @param[out] response 非空时写入应答指针；否则有应答时由本实现 `freeMessage`
	 * @return `MCP_SUCCESS`、`MCP_OBJECT_NOTFOUND`、`MCP_MESSAGEHANDLER_NOTFOUND` 等
	 */
	ErrorCode invokeHandler(ObjectInfo* objInfo, IMessage* &req, IMessage** response);

	/**
	 * @brief 系统消息回调（如 `SystemReady` 时从环境变量解析中继对象并绑定 `_messageRelayer`）。
	 * @param[in] msg 系统或其它消息
	 * @return 当前实现恒为 nullptr
	 */
	virtual const mcp::IMessage* handleMessage(const mcp::IMessage* msg);
private:
	/**
	 * @brief 工作线程主循环：等待队列、对每条任务逐个 `invokeHandler` 并释放消息。
	 */
	void run();

	/**
	 * @brief 将异步投递任务入队并唤醒线程；调用后 `msg` 置 nullptr，所有权转移给队列。
	 * @param[in] targets 接收方 `ObjectInfo*` 列表
	 * @param[in,out] msg 待入队消息；出参置空
	 */
	void enqueueMessage(std::vector<ObjectInfo*> targets, IMessage* &msg);

private:
	/** @brief 异步队列单元：一组目标 + 同一条消息。 */
	struct MessagePack
	{
		std::vector<ObjectInfo*> _target; 		//接收方列表
		const IMessage* _msg;             		//待投递消息（处理完后释放）
	};
	bool                         _isTerminate;                //请求线程退出	
	std::deque<MessagePack>      _messages;                   //异步任务队列
	std::mutex                   _mutex;                      //保护队列与 _isTerminate
	std::condition_variable      _cv;                         //队列非空或退出时唤醒
	std::unique_ptr<std::thread> _thread;                     //异步投递工作线程（入口 `run`）：构造时 `std::thread(&MessageCenter::run, this)` 启动，析构里 `join`；与 `_messages`、`_cv` 配合消费 `post`/`multicast`/`broadcast` 本地异步入队的任务。
	IMessageRelayer              *_messageRelayer = nullptr;  //可选的分布式消息中继。初始为 nullptr；收到 `SystemReady` 时按环境变量 `MCP_MESSAGE_RELAY`（缺省对象名同此字符串）`findObject` 绑定。未注册或查找失败则保持空，凡 `dstNodeID != 0` 及 join/leave 组等远程路径将失败。
};

MCP_END_NAMESPACE