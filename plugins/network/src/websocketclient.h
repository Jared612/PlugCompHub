/**
 * @file websocketclient.h
 * @brief IWebSocketClient 的 Boost.Beast 异步实现
 */
#pragma once

#include "network/iwebsocketclient.h"

#include <memory>
#include <mutex>
#include <string>

namespace pch {

struct WsClientSession;

class WebSocketClient : public IWebSocketClient
{
public:
	WebSocketClient() = default;
	virtual ~WebSocketClient();

	void setMessageCallback(MessageCallback cb) override;
	void setCloseCallback(CloseCallback cb) override;
	void connect(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, ConnectCallback cb) override;
	void send(const std::string& msg) override;
	void close() override;

private:
	friend struct WsClientSession;

	void notifyMessage(const std::string& msg);
	void notifyClosed();

	std::mutex _mutex;
	std::shared_ptr<WsClientSession> _session; // 当前连接会话（在 io 线程内访问）
	MessageCallback _msgCb;
	CloseCallback _closeCb;
};

}
