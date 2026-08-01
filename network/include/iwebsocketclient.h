/**
 * @file iWebSocketClient.h
 * @brief WebSocket 客户端抽象接口（基于异步回调）
 */
#pragma once

#include <functional>
#include <string>

namespace pch {

class IWebSocketClient
{
public:
	using MessageCallback = std::function<void(const std::string& msg)>;
	using ConnectCallback = std::function<void(bool success, const std::string& err_msg)>;
	using CloseCallback   = std::function<void()>;

	virtual ~IWebSocketClient() = default;

	virtual void setMessageCallback(MessageCallback cb) = 0;
	virtual void setCloseCallback(CloseCallback cb)   = 0;
	virtual void connect(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, ConnectCallback cb) = 0;
	virtual void send(const std::string& msg) = 0;
	virtual void close() = 0;
};

}
