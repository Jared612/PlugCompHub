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

	virtual void send(const char* data, size_t size, bool binary = false) = 0;
	virtual void sendText(const char* text) = 0;
	virtual void close() = 0;
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

	virtual void onOpen(WsOpenHandler handler) = 0;
	virtual void onMessage(WsMessageHandler handler) = 0;
	virtual void onClose(WsCloseHandler handler) = 0;

	/**
	 * @brief 向所有已连接客户端广播消息。
	 */
	virtual void broadcast(const char* data, size_t size, bool binary = false) = 0;
	virtual void broadcastText(const char* text) = 0;

	virtual size_t getSessionCount() const = 0;
	virtual bool isRunning() const = 0;
};

} // namespace pcx
