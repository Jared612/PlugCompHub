#pragma once

#include "ipcxwebsocketserver.h"

#include <boost/asio.hpp>
#include <boost/beast/websocket.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace pcx {

class PcxWebSocketServer : public IPcxWebSocketServer
{
public:
	struct SessionState;

public:
	PcxWebSocketServer() = default;
	~PcxWebSocketServer() override;

	bool start(const char* address, unsigned short port, const char* asioContextName) override;
	void stop() override;

	void onOpen(WsOpenHandler handler) override;
	void onMessage(WsMessageHandler handler) override;
	void onClose(WsCloseHandler handler) override;

	void broadcast(const char* data, size_t size, bool binary = false) override;
	void broadcastText(const char* text) override;

	size_t getSessionCount() const override;
	bool isRunning() const override;

private:
	void acceptLoop();
	void sessionLoop(std::shared_ptr<SessionState> session);
	void removeSession(uint64_t id);

	std::atomic<bool> _running{ false };
	std::atomic<uint64_t> _nextSessionId{ 1 };

	boost::asio::io_context* _ioContext{ nullptr };
	std::unique_ptr<boost::asio::ip::tcp::acceptor> _acceptor;
	std::thread _acceptThread;

	mutable std::mutex _mutex;
	WsOpenHandler _onOpen;
	WsMessageHandler _onMessage;
	WsCloseHandler _onClose;
	std::unordered_map<uint64_t, std::shared_ptr<SessionState>> _sessions;
};

}
