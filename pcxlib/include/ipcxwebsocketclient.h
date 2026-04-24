/**
 * @file ipcxwebsocketclient.h
 * @brief WebSocket 客户端抽象接口（长连接）。
 * @details 通过 IPcxAsioContext 驱动异步 I/O；connect 建立会话后可 send/close。
 */
#pragma once

#include <functional>
#include <string>

namespace pcx {

class IPcxWebSocketClient
{
public:
	using MessageCallback = std::function<void(const std::string& msg)>;
	using ConnectCallback = std::function<void(bool success, const std::string& err_msg)>;
	using CloseCallback   = std::function<void()>;

	virtual ~IPcxWebSocketClient() = default;

	virtual void setMessageCallback(MessageCallback cb) = 0;
	virtual void setCloseCallback(CloseCallback cb)   = 0;

	/**
	 * @param asioContextName 已 start 的 IPcxAsioContext 对象名
	 * @param target          握手路径，如 "/" 或 "/ws"
	 */
	virtual void connect(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, ConnectCallback cb) = 0;

	virtual void send(const std::string& msg) = 0;
	virtual void close()                      = 0;
};

} // namespace pcx
