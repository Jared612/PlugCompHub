#include "pcxwebsocketserver.h"

#include "netcommon.h"

#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <cstring>

namespace pcx {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

struct PcxWebSocketServer::SessionState
{
	uint64_t id{ 0 };
	std::shared_ptr<websocket::stream<tcp::socket>> ws;
	std::mutex writeMutex;
	std::atomic<bool> open{ true };
};

class WebSocketSessionHandle : public IWebSocketSession
{
public:
	explicit WebSocketSessionHandle(std::shared_ptr<PcxWebSocketServer::SessionState> state)
		: _state(std::move(state))
	{
	}

	void send(const char* data, size_t size, bool binary = false) override
	{
		auto state = _state.lock();
		if (!state || !state->open || !state->ws || data == nullptr) {
			return;
		}

		std::lock_guard<std::mutex> lk(state->writeMutex);
		boost::system::error_code ec;
		state->ws->binary(binary);
		state->ws->write(net::buffer(data, size), ec);
	}

	void sendText(const char* text) override
	{
		if (text == nullptr) {
			return;
		}
		send(text, std::strlen(text), false);
	}

	void close() override
	{
		auto state = _state.lock();
		if (!state || !state->ws) {
			return;
		}
		state->open = false;
		boost::system::error_code ec;
		state->ws->close(websocket::close_code::normal, ec);
	}

	uint64_t getId() const override
	{
		auto state = _state.lock();
		return state ? state->id : 0;
	}

private:
	std::weak_ptr<PcxWebSocketServer::SessionState> _state;
};

PcxWebSocketServer::~PcxWebSocketServer()
{
	stop();
}

bool PcxWebSocketServer::start(const char* address, unsigned short port, const char* asioContextName)
{
	if (_running) {
		return true;
	}
	if (address == nullptr || address[0] == '\0' || asioContextName == nullptr || asioContextName[0] == '\0') {
		return false;
	}

	_ioContext = resolveIoContext(asioContextName);
	if (_ioContext == nullptr) {
		return false;
	}

	boost::system::error_code ec;
	const auto ip = net::ip::make_address(address, ec);
	if (ec) {
		return false;
	}
	_acceptor.reset(new tcp::acceptor(*_ioContext));
	_acceptor->open(tcp::v4(), ec);
	if (ec) {
		return false;
	}
	_acceptor->set_option(net::socket_base::reuse_address(true), ec);
	if (ec) {
		return false;
	}
	_acceptor->bind({ ip, port }, ec);
	if (ec) {
		return false;
	}
	_acceptor->listen(net::socket_base::max_listen_connections, ec);
	if (ec) {
		return false;
	}

	_running = true;
	_acceptThread = std::thread([this]() { acceptLoop(); });
	return true;
}

void PcxWebSocketServer::stop()
{
	if (!_running) {
		return;
	}

	_running = false;
	if (_acceptor) {
		boost::system::error_code ec;
		_acceptor->cancel(ec);
		_acceptor->close(ec);
	}
	if (_acceptThread.joinable()) {
		_acceptThread.join();
	}

	std::unordered_map<uint64_t, std::shared_ptr<SessionState>> sessions;
	{
		std::lock_guard<std::mutex> lk(_mutex);
		sessions = _sessions;
		_sessions.clear();
	}
	for (auto& kv : sessions) {
		auto& state = kv.second;
		state->open = false;
		boost::system::error_code ec;
		if (state->ws) {
			state->ws->close(websocket::close_code::normal, ec);
		}
	}
}

void PcxWebSocketServer::onOpen(WsOpenHandler handler)
{
	std::lock_guard<std::mutex> lk(_mutex);
	_onOpen = std::move(handler);
}

void PcxWebSocketServer::onMessage(WsMessageHandler handler)
{
	std::lock_guard<std::mutex> lk(_mutex);
	_onMessage = std::move(handler);
}

void PcxWebSocketServer::onClose(WsCloseHandler handler)
{
	std::lock_guard<std::mutex> lk(_mutex);
	_onClose = std::move(handler);
}

void PcxWebSocketServer::broadcast(const char* data, size_t size, bool binary)
{
	if (data == nullptr) {
		return;
	}
	std::unordered_map<uint64_t, std::shared_ptr<SessionState>> sessions;
	{
		std::lock_guard<std::mutex> lk(_mutex);
		sessions = _sessions;
	}
	for (auto& kv : sessions) {
		auto& state = kv.second;
		if (!state->open || !state->ws) {
			continue;
		}
		{
			std::lock_guard<std::mutex> lk(state->writeMutex);
			boost::system::error_code ec;
			state->ws->binary(binary);
			state->ws->write(net::buffer(data, size), ec);
			if (ec) {
				state->open = false;
			}
		}
	}
}

void PcxWebSocketServer::broadcastText(const char* text)
{
	if (text == nullptr) {
		return;
	}
	broadcast(text, std::strlen(text), false);
}

size_t PcxWebSocketServer::getSessionCount() const
{
	std::lock_guard<std::mutex> lk(_mutex);
	return _sessions.size();
}

bool PcxWebSocketServer::isRunning() const
{
	return _running;
}

void PcxWebSocketServer::acceptLoop()
{
	while (_running && _acceptor) {
		boost::system::error_code ec;
		tcp::socket socket(*_ioContext);
		_acceptor->accept(socket, ec);
		if (ec) {
			if (_running) {
				continue;
			}
			break;
		}

		auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
		ws->accept(ec);
		if (ec) {
			continue;
		}

		auto session = std::make_shared<SessionState>();
		session->id = _nextSessionId.fetch_add(1);
		session->ws = ws;

		WsOpenHandler openCb;
		{
			std::lock_guard<std::mutex> lk(_mutex);
			_sessions[session->id] = session;
			openCb = _onOpen;
		}
		if (openCb) {
			WebSocketSessionHandle handle(session);
			openCb(handle);
		}

		std::thread([this, session]() { sessionLoop(session); }).detach();
	}
}

void PcxWebSocketServer::sessionLoop(std::shared_ptr<SessionState> session)
{
	while (_running && session->open) {
		beast::flat_buffer buffer;
		boost::system::error_code ec;
		{
			std::lock_guard<std::mutex> lk(session->writeMutex);
			session->ws->read(buffer, ec);
		}
		if (ec) {
			break;
		}

		auto msg = beast::buffers_to_string(buffer.data());
		WsMessageHandler msgCb;
		{
			std::lock_guard<std::mutex> lk(_mutex);
			msgCb = _onMessage;
		}
		if (msgCb) {
			WebSocketSessionHandle handle(session);
			msgCb(handle, msg.data(), msg.size(), true);
		}
	}
	session->open = false;
	removeSession(session->id);
}

void PcxWebSocketServer::removeSession(uint64_t id)
{
	WsCloseHandler closeCb;
	{
		std::lock_guard<std::mutex> lk(_mutex);
		_sessions.erase(id);
		closeCb = _onClose;
	}
	if (closeCb) {
		closeCb(id);
	}
}

}
