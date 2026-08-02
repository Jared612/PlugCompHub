/**
 * @file websocketserver.h
 * @brief IWebSocketServer 的 Boost.Beast 异步实现
 */
#pragma once

#include "network/iwebsocketserver.h"

#include <memory>

namespace pch {

struct WsListener;

class WebSocketServer : public IWebSocketServer
{
public:
	WebSocketServer() = default;
	virtual ~WebSocketServer() = default;

	bool start(const char* asioContextName, const std::string& host, const std::string& port, MessageHandler handler) override;
	void stop() override;

private:
	std::shared_ptr<WsListener> _listener;
};

}
