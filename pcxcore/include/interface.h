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


/**
 * @brief 进程内消息载荷与路由信息的只读视图。
 * @details 由 `IMessageCenter::allocMessage` 分配、`freeMessage` 释放；具体实现类由内核提供。
 */
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
	 * @brief 获取指定下标的接收者对象名。
	 * @param index 接收者索引，范围 `[0, getTargetsCount())`。
	 */
	virtual const char* getTarget(uint16_t index) const = 0;

	/**
	* @brief 获取此消息接收者列表个数
	*/
	virtual const uint16_t getTargetsCount() const = 0;

};

/**
 * @brief 组件侧消息处理入口：实现 `handleMessage` 即可接入 `IMessageCenter`。
 */
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
 * @details 所有权约定：当 `IMessageCenter` 内部将 `req`/`message` 转发给本中继实现时，
 *          所有权转交由 `MessageCenter` 负责：relayer 不得主动 `freeMessage`；
 *          MessageCenter 在 relayer 返回后统一释放消息。
*/
class IMessageRelayer
{
public:
	/**
	* @brief  同步发送接口
	*
	* @param  dstNodeID 目的组件NodeID
	* @param  target 目的组件名称
	* @param  req 发送消息；由调用方（MessageCenter）持有所有权，relayer 不应释放
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
	* @param  req 发送消息；由调用方（MessageCenter）持有所有权，relayer 不应释放
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
	* @param  req 发送消息；由调用方（MessageCenter）持有所有权，relayer 不应释放
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
	* @param  req 发送消息；由调用方（MessageCenter）持有所有权，relayer 不应释放
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

	/**
	 * @brief 获取组成员列表（分布式中继侧）。
	 * @param groupId 组 ID。
	 * @return 成员 ID 列表字符串，逗号分隔；生命周期由实现决定，调用方勿长期持有裸指针。
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
	 * @brief 本进程内异步广播到所有已注册对象。
	 * @param[in,out] message 待发送消息；投递后由消息中心释放，调用方勿再使用或释放。
	 * @param dstNodeID 保留/远端广播 ID（与 `IMessageRelayer` 协同时的占位参数）。
	 * @param msgPolicy 分发策略位（`DefaultMsgPolic` / `BreakOnError` / `ReverseOrder` 等）。
	 * @return PCX_SUCCESS 表示已入队或派发成功；否则为错误码。
	 */
	virtual ErrorCode broadcastMessageAsync(IMessage*& message, uint32_t dstNodeID = 0, uint32_t msgPolicy = DefaultMsgPolic) = 0;

	/**
	 * @brief 跨进程异步广播（经中继实现）。
	 * @param dstNodeID 目标节点或广播域 ID。
	 * @param[in,out] message 待发送消息；由消息中心/中继侧负责释放。
	 * @return PCX_SUCCESS 表示调用成功受理；具体语义依赖 `IMessageRelayer` 实现。
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

	/**
	 * @brief 获取本进程消息中心视角下的分组成员列表。
	 * @param groupId 组 ID。
	 * @return 成员 ID 列表字符串，逗号分隔；生命周期由实现决定。
	 */
	virtual const char* getGroupMembers(uint32_t groupId) = 0;

};

/** @brief 日志输出级别（数值顺序仅用于枚举，比较语义由 `LoggerManager` 决定）。 */
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
 * @brief 面向业务代码的日志书写接口（printf 风格可变参数）。
 * @note 线程安全由 `LoggerManager` 与具体后端共同保证；`fmt` 须为合法格式串。
 */
class ILogger
{
public:
	/** @brief 记录 Fatal 级别日志。 */
	virtual void fatal(const char* fmt, ...) = 0;
	/** @brief 记录 Error 级别日志。 */
	virtual void error(const char* fmt, ...) = 0;
	/** @brief 记录 Warning 级别日志。 */
	virtual void warn(const char* fmt, ...) = 0;
	/** @brief 记录 Information 级别日志。 */
	virtual void info(const char* fmt, ...) = 0;
	/** @brief 记录 Debug 级别日志。 */
	virtual void debug(const char* fmt, ...) = 0;
	/** @brief 记录 Trace 级别日志。 */
	virtual void trace(const char* fmt, ...) = 0;
};

/**
 * @brief 日志持久化后端：将已格式化的文本写入控制台/文件等。
 */
class ILoggerWrite
{
public:
	/**
	 * @brief 写入一条日志行。
	 * @param level 日志级别。
	 * @param logText 已格式化完整文本（含换行与否由实现决定）。
	 */
	virtual void writeLog(pcx::PcxLogLevel level, const char* logText) = 0;

	/** @brief 将缓冲数据刷到持久介质（若适用）。 */
	virtual void flush() = 0;
};

/**
 * @brief 日志格式化策略：将级别、logger 名、用户格式串格式化为缓冲区文本。
 */
class ILoggerFormat
{
public:
	/**
	 * @brief 格式化一条日志到调用方缓冲区。
	 * @param logBuffer 输出缓冲区。
	 * @param bufferSize 缓冲区大小（字节）。
	 * @param logName 逻辑 logger 名称（可包含层级 `.`）。
	 * @param level 本条日志级别。
	 * @param fmt printf 风格格式串。
	 * @param va 对应 `fmt` 的可变参数列表。
	 * @return 写入字符数（不含 `\0`），失败返回负值。
	 */
	virtual int formatLog(char* logBuffer, int bufferSize, const char* logName , pcx::PcxLogLevel level, const char* fmt, va_list va) = 0;
};

/**
 * @brief 一次查询返回的多个对象视图（由 `findObjectByInfo` 分配）。
 * @note 使用完毕须调用 `IObjectManager::freeObjectArray` 释放。
 */
class IObjectArray
{
public:
	/** @brief 虚析构：允许通过 `IObjectArray*` 交由 `freeObjectArray` 做 `delete`，避免切片 UB。 */
	virtual ~IObjectArray() = default;
	/** @return 对象个数。 */
	virtual unsigned int GetObjectCount() = 0;
	/**
	 * @param idx 下标，`[0, GetObjectCount())`。
	 * @return 对象裸指针；生命周期受 `ObjectManager` 管理，勿 `delete`。
	 */
	virtual void* GetObject(unsigned int idx) = 0;
};

/**
 * @brief 对象管理接口：与 `pcx::api` 及 `pluginit` 注入的实例一致。
 */
class IObjectManager
{
public:
	/**
	 * @brief 按组件 ID 创建具名全局对象并注册到管理器。
	 * @param componentID 已注册组件 ID。
	 * @param objName 全局唯一对象名；已存在则失败。
	 * @param initMsg 可选初始化消息（实现可忽略）。
	 * @param[out] errCode 失败时写入错误码，可为 nullptr。
	 * @param file 可选，调用点文件名（用于诊断）。
	 * @param line 可选，调用点行号。
	 * @return 成功返回对象指针；失败返回 nullptr。
	 */
	virtual void* createNamedObject(const char* componentID, const char* objName, IMessage* initMsg = nullptr, ErrorCode* errCode = nullptr, const char* file = nullptr, int line = 0) = 0;
	/**
	 * @brief 创建匿名对象（不参与按名查找）。
	 * @param componentID 已注册组件 ID。
	 * @param initMsg 可选初始化消息。
	 * @param[out] errCode 失败原因，可为 nullptr。
	 * @return 成功返回指针；须由调用方 `deleteObject` 释放（与具名对象策略不同）。
	 */
	virtual void* createObject(const char* componentID, IMessage* initMsg = nullptr, ErrorCode* errCode = nullptr, const char* file = nullptr, int line = 0) = 0;
	/**
	 * @brief 按地址销毁先前由本管理器创建的对象。
	 * @param obj 对象指针。
	 * @return PCX_SUCCESS 或错误码。
	 */
	virtual ErrorCode deleteObject(void* obj, const char* file = nullptr, int line = 0) = 0;
	/** @brief 按注册名查找对象；未找到返回 nullptr。 */
	virtual void* findObject(const char* objName) = 0;
	/**
	 * @brief 按组件元数据键值对过滤对象集合。
	 * @param key 键名数组。
	 * @param value 对应取值数组。
	 * @param count 键值对数量。
	 * @return 结果集；须 `freeObjectArray`。
	 */
	virtual IObjectArray* findObjectByInfo(const char* key[], const char* value[], int count) = 0;
	/** @brief 释放 `findObjectByInfo` 返回的数组对象。 */
	virtual ErrorCode freeObjectArray(IObjectArray* msg) = 0;
	/** @brief 查询对象注册名；未知指针可能返回 nullptr。 */
	virtual const char* getObjectName(void* obj) = 0;
};


PCX_END_NAMESPACE