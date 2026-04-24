/**
 * @file interface.h
 * @brief 对外可见的核心抽象：消息、消息中心、环境、日志等接口与相关常量。
 * @details 在 `pcxcomponent.h` 之上展开协议与虚接口；扩展功能或对接 PCX 时常包含本头。
 */

#pragma once
#include "pcxcomponent.h"
#include <list>
#include <string>

PCX_BEGIN_NAMESPACE

using ErrorCode = int;

// 对外暴露的协议常量定义
// 消息主类型定义
static const uint32_t SystemMessage      = 0xF0000000; // 系统消息
static const uint32_t CommandMessage     = 0xF0000001; // 命令消息
static const uint32_t EventMessage       = 0xF0000002; // 事件消息

// 系统生命周期消息码
static const uint32_t SystemObjectInit   = 0xFFFF0002; // 系统对象初始化
static const uint32_t SystemReady        = 0xFFFF0000; // 系统就绪
static const uint32_t SystemRun          = 0xFFFF0001; // 系统运行
static const uint32_t SystemStop         = 0xFFFFFFFE; // 系统停止
static const uint32_t SystemShutdown     = 0xFFFFFFFF; // 系统关闭

// 消息分发策略位定义
static const uint32_t DefaultMsgPolic    = 0x00000000; // 默认发送策略：顺序发送，有错误继续发送
static const uint32_t BreakOnError       = 0x00000001; // 出现错误立即返回
static const uint32_t ReverseOrder       = 0x00000004; // 按 target 逆序发送


// IMessage 消息协议，外部一般不改它的接口，只是按它来读数据。
class IMessage
{
public:
	virtual ~IMessage()
	{
	}

	/**
	* @brief 此消息的类型
	*/
	virtual uint32_t getType() const = 0;

	/**
	* @brief 此消息的命令值
	*/
	virtual uint32_t getCode() const = 0;

	/**
	* @brief 此消息的数据大小
	*/
	virtual uint32_t getSize() const = 0;

	/**
	* @brief 返回此消息的数据指针，可以将自己的数据拷贝到此指针指向的内存，
	* 数据可用大小为getSize()获取的大小
	* @return
	*/
	virtual unsigned char* const getData() const = 0;

	/**
	* @brief 获取此消息的发送者名称
	*/
	virtual const char* getSource() const = 0;

	/**
	* @brief 获取指定接收者
	* @param index接收者索引
	*/
	virtual const char* getTarget(uint16_t index) const = 0;

	/**
	* @brief 获取此消息接收者列表个数
	*/
	virtual const uint16_t getTargetsCount() const = 0;

};

// IMessageHandler 是外部组件接入 PCX 消息系统的“入口”。 外部最常见用法就是：你的组件继承它，然后实现 handleMessage 
class IMessageHandler
{
public:
	virtual ~IMessageHandler()
	{
	}

	/**
	* @brief 消息处理函数
	* @param msg - 发送到此对象的消息（不需要handleMessage内释放msg）
	* @return - 回复的消息
	*/
	virtual const pcx::IMessage* handleMessage(const pcx::IMessage* msg) = 0;
};

/**
 * @brief 分布式消息接口定义
*/
class IMessageRelayer
{
public:
	/**
	* @brief  同步发送接口
	*
	* @param  dstNodeID 目的组件NodeID
	* @param  target 目的组件名称
	* @param  req 发送消息 注：需要用户调freemessage释放
	* @param  rsp 响应消息 注：接口返回成功后，需要用户调freemessage释放
	* @param  timeout 超时时间，默认1000ms
	*
	* @return PCX_SUCCESS - 成功
			  其他        - 错误码
	*/
	virtual ErrorCode sendMessage(uint32_t dstNodeID, const char* target, pcx::IMessage* req, pcx::IMessage** rsp, uint32_t timeout = 1000) = 0;

	/**
	* @brief  异步发送接口
	*
	* @param  dstNodeID 目的组件NodeID
	* @param  target 目的组件名称
	* @param  req 发送消息 注：需要用户调freemessage释放
	*
	*注：异步消息的响应在handlemessage中
	* @return PCX_SUCCESS - 成功
			  其他        - 错误码
	*/
	virtual ErrorCode postMessage(uint32_t dstNodeID, const char* target, pcx::IMessage* req) = 0;

	/**
	* @brief  多播发送接口
	*
	* @param  dstNodeID 目的组件NodeID（节点ID/广播ID/组ID）
	* @param  targets 目的组件名称列表
	* @param  count   目的组件名称列表数量
	* @param  req 发送消息 注：需要用户调freemessage释放
	*
	*注：异步消息的响应在handlemessage中
	* @return PCX_SUCCESS - 成功
			  其他        - 错误码
	*/
	virtual ErrorCode multicastRemoteMessageAsync(uint32_t dstNodeID, const char* targets[], int count, pcx::IMessage*& req) = 0;

	/**
	* @brief  广播发送接口
	*
	* @param  dstNodeID 目的组件NodeID（节点ID/广播ID/组ID）
	* @param  req 发送消息 注：需要用户调freemessage释放
	*
	*注：异步消息的响应在handlemessage中
	* @return PCX_SUCCESS - 成功
			  其他        - 错误码
	*/
	virtual ErrorCode broadcastRemoteMessageAsync(uint32_t dstNodeID, pcx::IMessage*& req) = 0;

	/**
	* @brief 加入组
	* @param groupId - 组ID
	* @return 加入组的结果，PCX_SUCCESS为成功
	*/
	virtual ErrorCode joinGroup(uint32_t groupId) = 0;

	/**
	* @brief 离开组
	* @param groupId - 组ID
	* @return 加入组的结果，PCX_SUCCESS为成功
	*/
	virtual ErrorCode leaveGroup(uint32_t groupId) = 0;

	/*
	* @brief 获取组成员
	* @param groupId - 组ID
	* @return 返回当前组的成员的id列表，以字符串返回，用,隔开，例如"2345,1243,2341"
	*/
	virtual const char* getGroupMembers(uint32_t groupId) = 0;
};

/**
 * @brief 消息中心，可以用来同步或异步发送消息给进程内其他对象
*/
class IMessageCenter
{
public:
	/**
	 * @brief 创建一个message，根据传入的数据指针及长度将数据拷贝到IMessage的data中
	 *	如果data为nullptr则只申请空间不拷贝。
	 *  ！注意：通过此接口获取的message通过freeMessage释放
	 * @param code		- 消息码
	 * @param source	- 发送者名称
	 * @param dataLen	- 数据长度，数据指针为nullptr时为要申请的内存大小
	 * @param data		- 数据指针
	 * @param type		- 消息类型
	 * @return IMessage指针，申请失败返回nullptr
	*/
	virtual IMessage* allocMessage(uint32_t code, const char* source, uint32_t dataLen, void* data = nullptr, uint32_t type = 0) = 0;

	/**
	 * @brief 释放message指针及其data内存
	 * @param msg - 要释放的IMessage指针
	*/
	virtual ErrorCode freeMessage(IMessage* msg) = 0;

	/**
	 * @brief 同步发送消息到对象名称为target的组件
	 * @param target - 组件对象名称
	 * @param request -  发送的消息， ！注意：同步发送的消息由消息中心释放，调用者无需关心
	 * @param response - 回复的消息， ！注意：回复的消息指针需要调用此接口者释放
	 * @param timeout - 超时时间，单位：毫秒
	 * @param dstNodeID - 目标节点ID
	 * @return 发送返回值，PCX_SUCCESS为成功，失败返回错误码
	*/
	virtual ErrorCode sendMessage(const char* target, IMessage*& request, IMessage** response, uint32_t dstNodeID = 0, uint32_t timeout = 0) = 0;
	/**
	 * @brief 本进程内同步发送消息到对象名称属于group的组件，只要能查找到的对象都会发送信息，
	 * 但若其中有一个对象查找失败也会返回错误
	 * @param targets - 要发送的组件集合
	 * @param count - 对象集合大小
	 * @param message - 发送的消息，！注意：同步发送的消息由消息中心释放，调用者无需关心
	 * @param msgPolicy - 消息发送规则
	 * @param timeout - 超时时间，单位：毫秒
	 * @return 发送返回值，PCX_SUCCESS为成功
	*/
	virtual ErrorCode multicastLocalMessage(const char* targets[], int count, IMessage* &message, uint32_t msgPolicy = DefaultMsgPolic, uint32_t timeout = 0) = 0;

	/**
	 * @brief 本进程内发送消息给所有已注册的对象
	 * @param message -发送的消息，！注意：同步发送的消息由消息中心释放，调用者无需关心
	 * @param msgPolicy - 消息发送规则
	 * @param timeout - 超时时间，单位：毫秒
	 * @return 组件对象查找结果，PCX_SUCCESS为成功
	*/
	virtual ErrorCode broadcastLocalMessage(IMessage* &message, uint32_t msgPolicy = DefaultMsgPolic, uint32_t timeout = 0) = 0;

	/**
	 * @brief 异步发送消息到对象target
	 * @param target - 组件对象名称
	 * @param message - 发送的消息	！注意：异步发送的消息由消息中心释放，调用者无需关心
	 * @param dstNodeID - 目标节点ID
	 * @return 组件对象查找结果，PCX_SUCCESS为成功
	*/
	virtual ErrorCode postMessage(const char* target, IMessage*& message, uint32_t dstNodeID = 0) = 0;

	/**
	* @brief 跨进程异步发送消息到一组对象，只要能查找到的对象都会发送信息，
	* 但若其中有一个对象查找失败也会返回错误
	* @param srcNodeID - 目标节点ID
	* @param targets - 要发送的组件对象集合
	* @param count - 对象集合大小
	* @param message - 发送的消息	！注意：异步发送的消息由消息中心释放，调用者无需关心
	* @param dstNodeID - 广播ID
	* @param msgPolicy - 消息发送规则
	* @return 组件对象查找结果，PCX_SUCCESS为成功
	*/
	virtual ErrorCode multicastMessageAsync(const char* targets[], int count, IMessage*& message, uint32_t dstNodeID = 0,uint32_t msgPolicy = DefaultMsgPolic) = 0;


	/**
	* @brief 跨进程异步发送消息到一组对象，只要能查找到的对象都会发送信息，
	* 但若其中有一个对象查找失败也会返回错误
	* @param srcNodeID - 目标节点ID
	* @param targets - 要发送的组件对象集合
	* @param count - 对象集合大小
	* @param message - 发送的消息	！注意：异步发送的消息由消息中心释放，调用者无需关心
	* @return 组件对象查找结果，PCX_SUCCESS为成功
	*/
	virtual ErrorCode multicastRemoteMessageAsync(uint32_t dstNodeID, const char* targets[], int count, IMessage* &message) = 0;

	/**
	 * @brief 本进程内异步发送消息到所有已注册的对象
	 * @param dstNodeID - 广播ID
	 * @param message - 发送的消息 ！注意：异步发送的消息由消息管理器释放，调用者无需关心
	 * @param msgPolicy - 消息发送规则
	 * @return 组件对象查找结果，PCX_SUCCESS为成功
	*/
	virtual ErrorCode broadcastMessageAsync(IMessage*& message, uint32_t dstNodeID = 0, uint32_t msgPolicy = DefaultMsgPolic) = 0;
	/**
	 * @brief 本进程内异步发送消息到所有已注册的对象
	 * @param message - 发送的消息 ！注意：异步发送的消息由消息管理器释放，调用者无需关心
	 * @param msgPolicy - 消息发送规则
	 * @return 组件对象查找结果，PCX_SUCCESS为成功
	*/
	/**
	* @brief 跨进程异步发送消息到所有已注册的对象
	* @param dstNodeID - 广播ID
	* @param message - 发送的消息 ！注意：异步发送的消息由消息管理器释放，调用者无需关心
	* @return 组件对象查找结果，PCX_SUCCESS为成功
	*/
	virtual ErrorCode broadcastRemoteMessageAsync(uint32_t dstNodeID, IMessage* &message) = 0;


	/**
	* @brief 加入组
	* @param groupId - 组ID
	* @return 加入组的结果，PCX_SUCCESS为成功
	*/
	virtual ErrorCode joinGroup(uint32_t groupId) = 0;

	/**
	* @brief 离开组
	* @param groupId - 组ID
	* @return 加入组的结果，PCX_SUCCESS为成功
	*/
	virtual ErrorCode leaveGroup(uint32_t groupId) = 0;

	/*
	* @brief 获取组成员
	* @param groupId - 组ID
	* @return 返回当前组的成员的id列表，以字符串返回，用,隔开，例如"2345,1243,2341"
	*/
	virtual const char* getGroupMembers(uint32_t groupId) = 0;

};

// PCX 日志级别定义
enum class PcxLogLevel
{
	Trace,	
	Debug,
	Information,
	Warning,
	Error,
	Fatal,
	Off
};

/**
* @brief PCX日志书写层接口
*/
class ILogger
{
public:
	virtual void fatal(const char* fmt, ...) = 0;
	virtual void error(const char* fmt, ...) = 0;
	virtual void warn(const char* fmt, ...) = 0;
	virtual void info(const char* fmt, ...) = 0;
	virtual void debug(const char* fmt, ...) = 0;
	virtual void trace(const char* fmt, ...) = 0;
};

/**
* @brief PCX日志持久层接口
*/
class ILoggerWrite
{
public:
	virtual void writeLog(pcx::PcxLogLevel level, const char* logText) = 0;

	virtual void flush() = 0;
};

/**
* @brief PCX日志格式化层接口
*/
class ILoggerFormat
{
public:
	virtual int formatLog(char* logBuffer, int bufferSize, const char* logName , pcx::PcxLogLevel level, const char* fmt, va_list va) = 0;
};

/**
 * @brief 对象数组接口（一次返回多个对象；供 IObjectManager 使用）。
 */
class IObjectArray
{
public:
	virtual unsigned int GetObjectCount() = 0;
	virtual void* GetObject(unsigned int idx) = 0;
};

/**
 * @brief 对象管理接口（供 pcx::api 内联封装使用，与内核实现一致）。
 */
class IObjectManager
{
public:
	virtual void* createNamedObject(const char* componentID, const char* objName, IMessage* initMsg = nullptr, ErrorCode* errCode = nullptr, const char* file = nullptr, int line = 0) = 0;
	virtual void* createObject(const char* componentID, IMessage* initMsg = nullptr, ErrorCode* errCode = nullptr, const char* file = nullptr, int line = 0) = 0;
	virtual ErrorCode deleteObject(void* obj, const char* file = nullptr, int line = 0) = 0;
	virtual void* findObject(const char* objName) = 0;
	virtual IObjectArray* findObjectByInfo(const char* key[], const char* value[], int count) = 0;
	virtual ErrorCode freeObjectArray(IObjectArray* msg) = 0;
	virtual const char* getObjectName(void* obj) = 0;
};


PCX_END_NAMESPACE