#pragma once

#include "ipcxwebsocketclient.h"

#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>

#include <atomic>
#include <mutex>
#include <thread>

namespace pcx {

class PcxWebSocketClient : public IPcxWebSocketClient
{
public:
	PcxWebSocketClient() = default;
	~PcxWebSocketClient() override;

	void setMessageCallback(MessageCallback cb) override;
	void setCloseCallback(CloseCallback cb) override;

	void connect(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, ConnectCallback cb) override;

	void send(const std::string& msg) override;
	void close() override;

private:
	std::mutex _mutex;
	std::mutex _ioMutex;
	MessageCallback _onMessage;
	CloseCallback _onClose;
	std::thread _worker;
	std::atomic<bool> _running{ false };

	boost::asio::io_context* _ioContext{ nullptr };
	std::shared_ptr<boost::beast::websocket::stream<boost::asio::ip::tcp::socket>> _ws;
};

}
