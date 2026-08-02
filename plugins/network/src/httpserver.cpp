#include "httpserver.h"
#include "asiocontext.h"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <atomic>
#include <memory>
#include <string>

namespace pch {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

struct HttpSession : std::enable_shared_from_this<HttpSession>
{
	tcp::socket socket;
	beast::flat_buffer buffer;
	http::request<http::string_body> req;
	http::response<http::string_body> res;
	IHttpServer::RequestHandler handler;

	HttpSession(tcp::socket&& s, IHttpServer::RequestHandler h)
		: socket(std::move(s)), handler(std::move(h))
	{
	}

	void run()
	{
		auto self = shared_from_this();
		http::async_read(socket, buffer, req, [this, self](beast::error_code ec, std::size_t) {
			if (ec) {
				return; // 客户端断开或请求不完整，直接回收
			}
			handleRequest();
		});
	}

	void handleRequest()
	{
		auto self = shared_from_this();
		res.version(req.version());
		res.keep_alive(false);
		res.set(http::field::server, "PlugCompHub");
		res.set(http::field::content_type, "text/plain");
		try {
			if (handler) {
				std::string mstr(req.method_string().data(), req.method_string().size());
				std::string tstr(req.target().data(), req.target().size());
				char buf[4096];
				const int n = handler(mstr, tstr, req.body(), buf, static_cast<int>(sizeof(buf)));
				if (n > 0) {
					res.body().assign(buf, static_cast<std::size_t>(n));
					res.result(http::status::ok);
				} else {
					res.body() = "handler error";
					res.result(http::status::internal_server_error);
				}
			} else {
				res.body() = "no handler";
				res.result(http::status::ok);
			}
		} catch (const std::exception& e) {
			res.body() = e.what();
			res.result(http::status::internal_server_error);
		}
		res.prepare_payload();
		http::async_write(socket, res, [this, self](beast::error_code ec, std::size_t) {
			beast::error_code ignored;
			socket.shutdown(tcp::socket::shutdown_send, ignored);
		});
	}
};

} // namespace

struct HttpListener : std::enable_shared_from_this<HttpListener>
{
	tcp::acceptor acceptor;
	IHttpServer::RequestHandler handler;
	std::atomic<bool> stopped{ false };

	HttpListener(asio::io_context& io, tcp::endpoint ep, IHttpServer::RequestHandler h)
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
				std::make_shared<HttpSession>(std::move(socket), handler)->run();
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

bool HttpServer::start(const char* asioContextName, const std::string& host, const std::string& port, RequestHandler handler)
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

	auto listener = std::make_shared<HttpListener>(ctx->io(), ep, std::move(handler));
	if (!listener->ok()) {
		return false;
	}
	_listener = listener;
	listener->doAccept();
	return true;
}

void HttpServer::stop()
{
	if (_listener) {
		_listener->stop();
		_listener.reset();
	}
}

}
