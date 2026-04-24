/**
 * @file ipcxwebsocketserver.h
 * @brief WebSocket 服务器组件对外抽象接口。
 * @details 提供连接管理、消息收发、广播等能力；底层基于 Boost.Beast 实现。
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

namespace pcx {

/**
 * @brief 单个 WebSocket 连接的操作句柄。
 */
class IWebSocketSession
{
public:
	virtual ~IWebSocketSession() = default;

	/** @brief 发送一帧二进制或文本帧（由 `binary` 控制）。 */
	virtual void send(const char* data, size_t size, bool binary = false) = 0;
	/** @brief 发送一帧 UTF-8 文本。 */
	virtual void sendText(const char* text) = 0;
	/** @brief 主动关闭该会话。 */
	virtual void close() = 0;
	/** @return 会话稳定标识，用于 `onClose` 回调关联。 */
	virtual uint64_t getId() const = 0;
};

using WsOpenHandler    = std::function<void(IWebSocketSession&)>;
using WsMessageHandler = std::function<void(IWebSocketSession&, const char* data, size_t size, bool isText)>;
using WsCloseHandler   = std::function<void(uint64_t sessionId)>;

/**
 * @brief PCX WebSocket 服务器抽象接口。
 */
class IPcxWebSocketServer
{
public:
	virtual ~IPcxWebSocketServer() = default;

	/**
	 * @brief 启动监听。
	 * @param address 绑定地址，如 "0.0.0.0"。
	 * @param port    端口号。
	 * @param asioContextName 已创建的 IPcxAsioContext 对象名称。
	 * @return 成功返回 true。
	 */
	virtual bool start(const char* address, unsigned short port, const char* asioContextName) = 0;

	virtual void stop() = 0;

	/** @brief 注册新连接建立回调。 */
	virtual void onOpen(WsOpenHandler handler) = 0;
	/** @brief 注册收到消息帧回调。 */
	virtual void onMessage(WsMessageHandler handler) = 0;
	/** @brief 注册会话关闭回调。 */
	virtual void onClose(WsCloseHandler handler) = 0;

	/**
	 * @brief 向所有已连接客户端广播消息。
	 */
	virtual void broadcast(const char* data, size_t size, bool binary = false) = 0;
	/** @brief 向所有存活会话广播文本帧。 */
	virtual void broadcastText(const char* text) = 0;

	/** @return 当前连接会话数量。 */
	virtual size_t getSessionCount() const = 0;
	/** @return 是否正在监听。 */
	virtual bool isRunning() const = 0;
};

} // namespace pcx
