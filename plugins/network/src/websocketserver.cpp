#include "websocketserver.h"
#include "asiocontext.h"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <atomic>
#include <memory>
#include <string>

namespace pch {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

struct WsSession : std::enable_shared_from_this<WsSession>
{
	websocket::stream<tcp::socket> ws;
	beast::flat_buffer buffer;
	IWebSocketServer::MessageHandler handler;

	WsSession(tcp::socket&& s, IWebSocketServer::MessageHandler h)
		: ws(std::move(s)), handler(std::move(h))
	{
	}

	void run()
	{
		auto self = shared_from_this();
		ws.async_accept([this, self](beast::error_code ec) {
			if (ec) {
				return;
			}
			doRead();
		});
	}

	void doRead()
	{
		auto self = shared_from_this();
		ws.async_read(buffer, [this, self](beast::error_code ec, std::size_t) {
			if (ec) {
				return;
			}
			const std::string msg = beast::buffers_to_string(buffer.data());
			buffer.consume(buffer.size());
			if (handler) {
				handler(msg, [self](const std::string& reply) { self->send(reply); });
			}
			doRead();
		});
	}

	void send(const std::string& msg)
	{
		auto self = shared_from_this();
		auto data = std::make_shared<std::string>(msg);
		asio::post(ws.get_executor(), [this, self, data]() {
			ws.text(true);
			ws.async_write(asio::buffer(*data), [self, data](beast::error_code, std::size_t) {});
		});
	}
};

} // namespace

struct WsListener : std::enable_shared_from_this<WsListener>
{
	tcp::acceptor acceptor;
	IWebSocketServer::MessageHandler handler;
	std::atomic<bool> stopped{ false };

	WsListener(asio::io_context& io, tcp::endpoint ep, IWebSocketServer::MessageHandler h)
		: acceptor(io), handler(std::move(h))
	{
		beast::error_code ec;
		acceptor.open(ep.protocol(), ec);
		if (!ec) {
			acceptor.set_option(asio::socket_base::reuse_address(true), ec);
		}
		if (!ec) {
			acceptor.bind(ep, ec);
		}
		if (!ec) {
			acceptor.listen(asio::socket_base::max_listen_connections, ec);
		}
		if (ec) {
			_error = ec.message();
			beast::error_code ignored;
			acceptor.close(ignored);
		}
	}

	bool ok() const { return _error.empty(); }
	const std::string& error() const { return _error; }

	void doAccept()
	{
		auto self = shared_from_this();
		acceptor.async_accept([this, self](beast::error_code ec, tcp::socket socket) {
			if (!ec) {
				std::make_shared<WsSession>(std::move(socket), handler)->run();
			}
			if (!stopped.load()) {
				doAccept();
			}
		});
	}

	void stop()
	{
		stopped.store(true);
		beast::error_code ec;
		acceptor.close(ec);
	}

private:
	std::string _error;
};

bool WebSocketServer::start(const char* asioContextName, const std::string& host, const std::string& port, MessageHandler handler)
{
	if (host.empty() || port.empty()) {
		return false;
	}
	if (_listener) {
		stop();
	}

	auto ctx = AsioContextManager::instance().get(asioContextName);
	tcp::resolver resolver(ctx->io());
	beast::error_code ec;
	auto results = resolver.resolve(host, port, ec);
	if (ec || results.empty()) {
		return false;
	}
	tcp::endpoint ep = results.begin()->endpoint();

	auto listener = std::make_shared<WsListener>(ctx->io(), ep, std::move(handler));
	if (!listener->ok()) {
		return false;
	}
	_listener = listener;
	listener->doAccept();
	return true;
}

void WebSocketServer::stop()
{
	if (_listener) {
		_listener->stop();
		_listener.reset();
	}
}

}
