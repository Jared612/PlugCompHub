#include "websocketclient.h"
#include "asiocontext.h"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <memory>
#include <string>
#include <utility>

namespace pch {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

} // namespace

struct WsClientSession : std::enable_shared_from_this<WsClientSession>
{
	tcp::resolver resolver;
	websocket::stream<tcp::socket> ws;
	beast::flat_buffer buffer;
	std::string host;
	std::string port;
	std::string target;
	WebSocketClient* owner;

	WsClientSession(asio::io_context& io, WebSocketClient* o, std::string h, std::string p, std::string t)
		: resolver(io), ws(io), host(std::move(h)), port(std::move(p)), target(std::move(t)), owner(o)
	{
	}

	void start(IWebSocketClient::ConnectCallback cb)
	{
		auto self = shared_from_this();
		resolver.async_resolve(host, port, [this, self, cb](beast::error_code ec, tcp::resolver::results_type results) {
			if (ec) {
				if (cb) {
					cb(false, "resolve: " + ec.message());
				}
				return;
			}
			asio::async_connect(ws.next_layer(), results, [this, self, cb](beast::error_code ec2, tcp::endpoint) {
				if (ec2) {
					if (cb) {
						cb(false, "connect: " + ec2.message());
					}
					return;
				}
				ws.async_handshake(host, target, [this, self, cb](beast::error_code ec3) {
					if (ec3) {
						if (cb) {
							cb(false, "handshake: " + ec3.message());
						}
						return;
					}
					if (cb) {
						cb(true, std::string());
					}
					doRead();
				});
			});
		});
	}

	void doRead()
	{
		auto self = shared_from_this();
		ws.async_read(buffer, [this, self](beast::error_code ec, std::size_t) {
			if (ec) {
				if (owner) {
					owner->notifyClosed();
				}
				return;
			}
			if (owner) {
				owner->notifyMessage(beast::buffers_to_string(buffer.data()));
			}
			buffer.consume(buffer.size());
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

	void close()
	{
		auto self = shared_from_this();
		asio::post(ws.get_executor(), [this, self]() {
			beast::error_code ec;
			ws.close(websocket::close_code::normal, ec);
		});
	}
};

WebSocketClient::~WebSocketClient()
{
	std::lock_guard<std::mutex> lk(_mutex);
	if (_session) {
		_session->owner = nullptr;
		_session->close();
	}
}

void WebSocketClient::setMessageCallback(MessageCallback cb)
{
	std::lock_guard<std::mutex> lk(_mutex);
	_msgCb = std::move(cb);
}

void WebSocketClient::setCloseCallback(CloseCallback cb)
{
	std::lock_guard<std::mutex> lk(_mutex);
	_closeCb = std::move(cb);
}

void WebSocketClient::notifyMessage(const std::string& msg)
{
	MessageCallback cb;
	{
		std::lock_guard<std::mutex> lk(_mutex);
		cb = _msgCb;
	}
	if (cb) {
		cb(msg);
	}
}

void WebSocketClient::notifyClosed()
{
	CloseCallback cb;
	{
		std::lock_guard<std::mutex> lk(_mutex);
		cb = _closeCb;
		_session.reset();
	}
	if (cb) {
		cb();
	}
}

void WebSocketClient::connect(const char* asioContextName, const std::string& host, const std::string& port,
	const std::string& target, ConnectCallback cb)
{
	if (host.empty() || port.empty()) {
		if (cb) {
			cb(false, "host/port is empty");
		}
		return;
	}

	auto ctx = AsioContextManager::instance().get(asioContextName);
	auto session = std::make_shared<WsClientSession>(ctx->io(), this, host, port, target);
	{
		std::lock_guard<std::mutex> lk(_mutex);
		_session = session;
	}
	session->start(std::move(cb));
}

void WebSocketClient::send(const std::string& msg)
{
	std::lock_guard<std::mutex> lk(_mutex);
	if (_session) {
		_session->send(msg);
	}
}

void WebSocketClient::close()
{
	std::lock_guard<std::mutex> lk(_mutex);
	if (_session) {
		_session->close();
	}
}

}
