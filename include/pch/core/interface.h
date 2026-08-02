/**
 * @file interface.h
 * @brief 面向外部的核心抽象：消息、消息中心、组件/插件/日志/环境管理接口及相关常量
 * @details 在 componentinfo.h 基础上扩展了协议定义和虚接口；在扩展或集成 PCH 时包含此头文件
 *
 * ABI 稳定性规则（重要）：
 * - 已发布的接口类（IMessage / IMessageCenter / IObjectManager / IPluginManager /
 *   IComponentManager / ILoggerManager / IEnvironment 等）禁止新增/删除虚函数、
 *   禁止修改已存在函数的签名——任何此类变更都会破坏插件二进制兼容，必须递增
 *   PCH_ABI_VERSION（见 coreexport.h）并要求插件与 core 同步重编译。
 * - 跨 DLL 边界只应传递内置类型、同一工具链下的纯虚接口指针和 const char*。
 */

#pragma once
#include "componentinfo.h"
#include <list>
#include <string>

PCH_BEGIN_NAMESPACE

using ErrorCode = int;

// 公开协议常量定义
// 消息主类型定义
static const uint32_t SystemMessage      = 0xF0000000; // 系统消息
static const uint32_t CommandMessage     = 0xF0000001; // 命令消息
static const uint32_t EventMessage       = 0xF0000002; // 事件消息

// 系统生命周期消息
static const uint32_t SystemObjectInit   = 0xFFFF0002; // 系统对象初始化
static const uint32_t SystemReady        = 0xFFFF0000; // 系统就绪
static const uint32_t SystemRun          = 0xFFFF0001; // 系统运行中
static const uint32_t SystemStop         = 0xFFFFFFFE; // 系统停止
static const uint32_t SystemShutdown     = 0xFFFFFFFF; // 系统关闭

// 消息分发策略位定义
static const uint32_t DefaultMsgPolic    = 0x00000000; // 默认分发：顺序执行，出错继续
static const uint32_t BreakOnError       = 0x00000001; // 出错立即返回
static const uint32_t ReverseOrder       = 0x00000004; // 按逆序发送到目标

/**
 * @brief 进程中消息载荷和路由信息的只读视图
 * @details 由 IMessageCenter::allocMessage 分配，freeMessage 释放；具体实现由内核提供
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
	* @brief 此消息的命令码
	*/
	virtual uint32_t getCode() const = 0;

	/**
	* @brief 此消息的数据大小
	*/
	virtual uint32_t getSize() const = 0;

	/**
	* @brief 返回此消息的数据指针，调用方可将数据拷贝到该内存
	* 可用数据大小为 getSize() 返回的值
	* @return 数据指针
	*/
	virtual unsigned char* getData() const = 0;

	/**
	* @brief 获取此消息的发送者名称
	*/
	virtual const char* getSource() const = 0;

	/**
	 * @brief 获取指定索引处的接收者对象名
	 * @param index 接收者索引，范围 [0, getTargetsCount())
	 */
	virtual const char* getTarget(uint16_t index) const = 0;

	/**
	* @brief 获取此消息的接收者数量
	*/
	virtual uint16_t getTargetsCount() const = 0;

};

/**
 * @brief 组件侧消息处理入口：实现 handleMessage 以连接到 IMessageCenter
 */
class IMessageHandler
{
public:
	virtual ~IMessageHandler()
	{
	}

	/**
	* @brief 消息处理函数
	* @param msg - 发送到此对象的消息（不要在 handleMessage 内释放 msg）
	* @return - 回复消息
	*/
	virtual const pch::IMessage* handleMessage(const pch::IMessage* msg) = 0;
};

/**
 * @brief 分布式消息中继接口定义
 * @details 所有权约定：当 IMessageCenter 将 req/消息转发到此中继实现时，
 *          所有权转移；MessageCenter 负责：中继器不得主动 freeMessage。
 *          MessageCenter 在中继器返回后统一释放消息。
 */
class IMessageRelayer
{
public:
	/**
	* @brief  同步发送接口
	*
	* @param  dstNodeID 目标组件 NodeID
	* @param  target 目标组件名称
	* @param  req 发送消息；由调用方（MessageCenter）拥有，中继器不应释放
	* @param  rsp 响应消息。注意：成功返回后，调用方必须通过 freeMessage 释放
	* @param  timeout 超时时间，默认 1000ms
	*
	* @return PCH_SUCCESS - 成功
	          other        - 错误码
	*/
	virtual ErrorCode sendMessage(uint32_t dstNodeID, const char* target, pch::IMessage* req, pch::IMessage** rsp, uint32_t timeout = 1000) = 0;

	/**
	* @brief  异步发送接口
	*
	* @param  dstNodeID 目标组件 NodeID
	* @param  target 目标组件名称
	* @param  req 发送消息；由调用方（MessageCenter）拥有，中继器不应释放
	*
	* 注意：异步消息的响应在 handleMessage 中
	* @return PCH_SUCCESS - 成功
	          other        - 错误码
	*/
	virtual ErrorCode postMessage(uint32_t dstNodeID, const char* target, pch::IMessage* req) = 0;

	/**
	* @brief  组播发送接口
	*
	* @param  dstNodeID 目标组件 NodeID（节点 ID / 广播 ID / 组 ID）
	* @param  targets 目标组件名称列表
	* @param  count   目标组件名称数量
	* @param  req 发送消息；由调用方（MessageCenter）拥有，中继器不应释放
	*
	* 注意：异步消息的响应在 handleMessage 中
	* @return PCH_SUCCESS - 成功
	          other        - 错误码
	*/
	virtual ErrorCode multicastRemoteMessageAsync(uint32_t dstNodeID, const char* targets[], int count, pch::IMessage*& req) = 0;

	/**
	* @brief  广播发送接口
	*
	* @param  dstNodeID 目标组件 NodeID（节点 ID / 广播 ID / 组 ID）
	* @param  req 发送消息；由调用方（MessageCenter）拥有，中继器不应释放
	*
	* 注意：异步消息的响应在 handleMessage 中
	* @return PCH_SUCCESS - 成功
	          other        - 错误码
	*/
	virtual ErrorCode broadcastRemoteMessageAsync(uint32_t dstNodeID, pch::IMessage*& req) = 0;

	/**
	* @brief 加入一个组
	* @param groupId - 组 ID
	* @return 加入组的结果，PCH_SUCCESS 表示成功
	*/
	virtual ErrorCode joinGroup(uint32_t groupId) = 0;

	/**
	* @brief 离开一个组
	* @param groupId - 组 ID
	* @return 离开组的结果，PCH_SUCCESS 表示成功
	*/
	virtual ErrorCode leaveGroup(uint32_t groupId) = 0;

	/**
	 * @brief 获取组成员列表（分布式中继侧）
	 * @param groupId 组 ID
	 * @return 逗号分隔的成员 ID 列表字符串；生命周期由实现决定，调用方不应长期持有原始指针
	 */
	virtual const char* getGroupMembers(uint32_t groupId) = 0;
};

/**
 * @brief 消息中心，可同步或异步地向进程内其他对象发送消息
 */
class IMessageCenter
{
public:
	/**
	 * @brief 创建消息，从指针和长度拷贝数据到 IMessage 的数据区
	 *        如果 data 为 nullptr，只分配空间而不拷贝。
	 *        注意：通过此接口获取的消息必须通过 freeMessage 释放
	 * @param code      - 消息码
	 * @param source    - 发送者名称
	 * @param dataLen   - 数据长度；当 data 为 nullptr 时，此为要分配的内存大小
	 * @param data      - 数据指针
	 * @param type      - 消息类型
	 * @return IMessage 指针，分配失败返回 nullptr
	*/
	virtual IMessage* allocMessage(uint32_t code, const char* source, uint32_t dataLen, void* data = nullptr, uint32_t type = 0) = 0;

	/**
	 * @brief 释放消息指针及其数据内存
	 * @param msg - 要释放的 IMessage 指针
	*/
	virtual ErrorCode freeMessage(IMessage* msg) = 0;

	/**
	 * @brief 同步发送消息到对象名为 target 的组件
	 * @param target - 组件对象名
	 * @param request -  要发送的消息。注意：同步发送的消息由消息中心释放，调用方无需关心
	 * @param response - 回复消息。注意：回复消息指针必须由此接口的调用方释放
	 * @param timeout - 超时时间，单位：毫秒
	 * @param dstNodeID - 目标节点 ID
	 * @return 发送结果，PCH_SUCCESS 表示成功，失败返回错误码
	*/
	virtual ErrorCode sendMessage(const char* target, IMessage*& request, IMessage** response, uint32_t dstNodeID = 0, uint32_t timeout = 0) = 0;
	/**
	 * @brief 同步发送消息到进程内组中指定名称的组件。
	 *        发送给所有找到的对象，但只要有一个对象查找失败则返回错误。
	 * @param targets - 要发送的组件集合
	 * @param count - 对象集合的大小
	 * @param message - 要发送的消息。注意：同步发送的消息由消息中心释放，调用方无需关心
	 * @param msgPolicy - 消息分发策略
	 * @param timeout - 超时时间，单位：毫秒
	 * @return 发送结果，PCH_SUCCESS 表示成功
	*/
	virtual ErrorCode multicastLocalMessage(const char* targets[], int count, IMessage* &message, uint32_t msgPolicy = DefaultMsgPolic, uint32_t timeout = 0) = 0;

	/**
	 * @brief 发送消息到进程内所有已注册对象
	 * @param message - 要发送的消息。注意：同步发送的消息由消息中心释放，调用方无需关心
	 * @param msgPolicy - 消息分发策略
	 * @param timeout - 超时时间，单位：毫秒
	 * @return 组件对象查找结果，PCH_SUCCESS 表示成功
	*/
	virtual ErrorCode broadcastLocalMessage(IMessage* &message, uint32_t msgPolicy = DefaultMsgPolic, uint32_t timeout = 0) = 0;

	/**
	 * @brief 异步发送消息到目标对象
	 * @param target - 组件对象名
	 * @param message - 要发送的消息。注意：异步消息由消息中心释放，调用方无需关心
	 * @param dstNodeID - 目标节点 ID
	 * @return 组件对象查找结果，PCH_SUCCESS 表示成功
	*/
	virtual ErrorCode postMessage(const char* target, IMessage*& message, uint32_t dstNodeID = 0) = 0;

	/**
	* @brief 跨进程异步发送消息到一组对象。发送给所有找到的对象，
	*        但只要有一个对象查找失败则返回错误。
	* @param srcNodeID - 目标节点 ID
	* @param targets - 要发送的组件对象集合
	* @param count - 对象集合的大小
	* @param message - 要发送的消息。注意：异步消息由消息中心释放，调用方无需关心
	* @param dstNodeID - 广播 ID
	* @param msgPolicy - 消息分发策略
	* @return 组件对象查找结果，PCH_SUCCESS 表示成功
	*/
	virtual ErrorCode multicastMessageAsync(const char* targets[], int count, IMessage*& message, uint32_t dstNodeID = 0,uint32_t msgPolicy = DefaultMsgPolic) = 0;


	/**
	* @brief 跨进程异步发送消息到一组对象。发送给所有找到的对象，
	*        但只要有一个对象查找失败则返回错误。
	* @param srcNodeID - 目标节点 ID
	* @param targets - 要发送的组件对象集合
	* @param count - 对象集合的大小
	* @param message - 要发送的消息。注意：异步消息由消息中心释放，调用方无需关心
	* @return 组件对象查找结果，PCH_SUCCESS 表示成功
	*/
	virtual ErrorCode multicastRemoteMessageAsync(uint32_t dstNodeID, const char* targets[], int count, IMessage* &message) = 0;

	/**
	 * @brief 进程内异步广播到所有已注册对象
	 * @param[in,out] message 要发送的消息；分发后由消息中心释放，调用方不得重用或释放
	 * @param dstNodeID 保留/远程广播 ID（与 IMessageRelayer 配合时的占位符）
	 * @param msgPolicy 分发策略位（DefaultMsgPolic / BreakOnError / ReverseOrder 等）
	 * @return PCH_SUCCESS 表示已入队或分发成功；否则返回错误码
	 */
	virtual ErrorCode broadcastMessageAsync(IMessage*& message, uint32_t dstNodeID = 0, uint32_t msgPolicy = DefaultMsgPolic) = 0;

	/**
	 * @brief 跨进程异步广播（通过中继实现）
	 * @param dstNodeID 目标节点或广播域 ID
	 * @param[in,out] message 要发送的消息；由消息中心/中继侧释放
	 * @return PCH_SUCCESS 表示已接受；具体语义取决于 IMessageRelayer 实现
	 */
	virtual ErrorCode broadcastRemoteMessageAsync(uint32_t dstNodeID, IMessage* &message) = 0;


	/**
	* @brief 加入一个组
	* @param groupId - 组 ID
	* @return 加入组的结果，PCH_SUCCESS 表示成功
	*/
	virtual ErrorCode joinGroup(uint32_t groupId) = 0;

	/**
	* @brief 离开一个组
	* @param groupId - 组 ID
	* @return 离开组的结果，PCH_SUCCESS 表示成功
	*/
	virtual ErrorCode leaveGroup(uint32_t groupId) = 0;

	/**
	 * @brief 从本进程消息中心视角获取组成员列表
	 * @param groupId 组 ID
	 * @return 逗号分隔的成员 ID 列表字符串；生命周期由实现决定
	 */
	virtual const char* getGroupMembers(uint32_t groupId) = 0;

};

/** @brief 日志输出级别（仅用于枚举排序；比较语义由 LoggerManager 决定） */
enum class LogLevel
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
 * @brief 业务代码的日志写入接口（printf 风格变参）
 * @note 线程安全由 LoggerManager 和具体后端共同保证；fmt 必须为有效格式字符串
 */
class ILogger
{
public:
	/** @brief 记录 Fatal 级别日志 */
	virtual void fatal(const char* fmt, ...) = 0;
	/** @brief 记录 Error 级别日志 */
	virtual void error(const char* fmt, ...) = 0;
	/** @brief 记录 Warning 级别日志 */
	virtual void warn(const char* fmt, ...) = 0;
	/** @brief 记录 Information 级别日志 */
	virtual void info(const char* fmt, ...) = 0;
	/** @brief 记录 Debug 级别日志 */
	virtual void debug(const char* fmt, ...) = 0;
	/** @brief 记录 Trace 级别日志 */
	virtual void trace(const char* fmt, ...) = 0;
};

/**
 * @brief 日志持久化后端：将格式化文本写入控制台、文件等
 */
class ILoggerWrite
{
public:
	/**
	 * @brief 写入一行日志
	 * @param level 日志级别
	 * @param logText 完整格式化文本（是否包含换行取决于实现）
	 */
	virtual void writeLog(pch::LogLevel level, const char* logText) = 0;

	/** @brief 将缓冲数据刷入持久化介质（如适用） */
	virtual void flush() = 0;
};

/**
 * @brief 日志格式化策略：将级别、日志名、用户格式字符串格式化为缓冲区文本
 */
class ILoggerFormat
{
public:
	/**
	 * @brief 将日志条目格式化到调用方缓冲区
	 * @param logBuffer 输出缓冲区
	 * @param bufferSize 缓冲区大小（字节）
	 * @param logName 逻辑日志名（可包含 '.' 层级）
	 * @param level 此条目的日志级别
	 * @param fmt printf 风格格式字符串
	 * @param va 与 fmt 对应的变参列表
	 * @return 写入的字符数（不含 \\0），失败返回负数
	 */
	virtual int formatLog(char* logBuffer, int bufferSize, const char* logName , pch::LogLevel level, const char* fmt, va_list va) = 0;
};

/**
 * @brief 单次查询返回的多对象视图（由 findObjectByInfo 分配）
 * @note 使用后必须调用 IObjectManager::freeObjectArray 释放
 */
class IObjectArray
{
public:
	/** @brief 虚析构函数：允许通过 freeObjectArray 以 IObjectArray* 删除，避免切片 UB */
	virtual ~IObjectArray() = default;
	/** @return 对象数量 */
	virtual unsigned int GetObjectCount() = 0;
	/**
	 * @param idx 索引，[0, GetObjectCount())
	 * @return 原始对象指针；生命周期由 ObjectManager 管理，请勿删除
	 */
	virtual void* GetObject(unsigned int idx) = 0;
};

/**
 * @brief 对象管理接口：与 pch::api 和 pluginit 注入的实例一致
 */
class IObjectManager
{
public:
	/**
	 * @brief 按组件 ID 创建命名全局对象并注册到管理器
	 * @param componentID 已注册的组件 ID
	 * @param objName 全局唯一的对象名；若已存在则失败
	 * @param initMsg 可选的初始化消息（实现可能忽略）
	 * @param[out] errCode 失败时写入错误码，可为 nullptr
	 * @param file 可选的调用方文件名（用于诊断）
	 * @param line 可选的调用方行号
	 * @return 成功返回对象指针；失败返回 nullptr
	 * @note initMsg 仅作为初始化消息投递给对象，消息中心不接管其所有权；
	 *       调用方负责在创建完成后释放（与 sendMessage 的"中心释放"语义不同）
	 */
	virtual void* createNamedObject(const char* componentID, const char* objName, IMessage* initMsg = nullptr, ErrorCode* errCode = nullptr, const char* file = nullptr, int line = 0) = 0;
	/**
	 * @brief 创建匿名对象（不参与按名称查找）
	 * @param componentID 已注册的组件 ID
	 * @param initMsg 可选的初始化消息
	 * @param[out] errCode 失败原因，可为 nullptr
	 * @return 成功返回指针；调用方必须通过 deleteObject 释放（与命名对象不同）
	 * @note initMsg 所有权同 createNamedObject：由调用方负责释放
	 */
	virtual void* createObject(const char* componentID, IMessage* initMsg = nullptr, ErrorCode* errCode = nullptr, const char* file = nullptr, int line = 0) = 0;
	/**
	 * @brief 按地址销毁由此管理器先前创建的对象
	 * @param obj 对象指针
	 * @return PCH_SUCCESS 或错误码
	 */
	virtual ErrorCode deleteObject(void* obj, const char* file = nullptr, int line = 0) = 0;
	/** @brief 按注册名称查找对象；未找到返回 nullptr */
	virtual void* findObject(const char* objName) = 0;
	/**
	 * @brief 按组件元数据的键值对过滤对象集合
	 * @param key 键名数组
	 * @param value 对应值数组
	 * @param count 键值对数量
	 * @return 结果集；使用 freeObjectArray 释放
	 */
	virtual IObjectArray* findObjectByInfo(const char* key[], const char* value[], int count) = 0;
	/** @brief 释放由 findObjectByInfo 返回的数组对象 */
	virtual ErrorCode freeObjectArray(IObjectArray* msg) = 0;
	/** @brief 查询对象注册名称；未知指针可能返回 nullptr */
	virtual const char* getObjectName(void* obj) = 0;
};


/**
 * @brief 已加载插件的信息抽象（宿主/应用框架侧可获取）
 */
class IPlugin
{
public:
	virtual const char* getName() = 0;
	virtual const char* getVersion() = 0;
	virtual const char* getDescription() = 0;
	virtual const char* getPath() = 0;
};

/**
 * @brief 插件管理器接口：加载/卸载动态插件库
 */
class IPluginManager
{
public:
	virtual IPlugin* loadPlugin(const char* path) = 0;
	virtual ErrorCode unloadPlugin(IPlugin* plugin) = 0;
};

/**
 * @brief 组件管理器接口：维护组件注册表
 */
class IComponentManager
{
public:
	virtual ErrorCode registerComponent(ComponentInfo* compInfo)     = 0;  // 注册单个组件
	virtual ErrorCode registerComponents(ComponentInfo** cmptable)   = 0;  // 批量注册组件
	virtual ErrorCode unregisterComponent(ComponentInfo* compInfo)   = 0;  // 注销单个组件（按信息）
	virtual ErrorCode unregisterComponents(ComponentInfo** cmptable) = 0;  // 批量注销组件
	virtual ErrorCode unregisterComponent(const char* componentID)   = 0;  // 注销单个组件（按 ID）
	virtual ComponentInfo* getComponentInf(const char* componentID)  = 0;  // 按 ID 查询组件信息
};

/**
 * @brief 日志管理器接口：默认/具名日志器与后端管理
 */
class ILoggerManager
{
public:
	virtual ILogger* getLogger(const char* logName)                                                            = 0;  // 按指定名称获取日志器
	virtual ILogger* getDefaultLogger()                                                                        = 0;  // 获取默认日志器
	virtual bool setDefaultLoggerLevel(LogLevel level)                                                      = 0;  // 设置默认日志器级别
	virtual bool setDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr)               = 0;  // 设置默认日志器输出
	virtual bool addDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr)               = 0;  // 添加默认日志器输出
	virtual bool setLoggerLevel(const char* logName, LogLevel level)                                        = 0;  // 设置指定日志器级别
	virtual bool setLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr) = 0;  // 设置指定日志器输出
	virtual bool addLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr) = 0;  // 添加指定日志器输出
};

/**
 * @brief 环境变量接口：读取/设置进程环境
 */
class IEnvironment
{
public:
	virtual const char* get(const char* key, ErrorCode* errCode = nullptr) = 0;  // 读取环境变量
	virtual ErrorCode set(const char* key, const char* value)              = 0;  // 设置环境变量
};


PCH_END_NAMESPACE
