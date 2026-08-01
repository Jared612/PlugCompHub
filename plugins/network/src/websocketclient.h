#pragma once

#include "network/iwebsocketclient.h"
#include "httplib.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace pch {

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
	void workerLoop(const std::string& url, ConnectCallback cb);

	std::mutex _mutex;
	std::unique_ptr<httplib::ws::WebSocketClient> _ws;
	std::thread _thread;
	std::atomic<bool> _running{false};

	MessageCallback _onMessage;
	CloseCallback   _onClose;
};

}
