#include "httpclient.h"
#include "asiocontext.h"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <memory>

namespace pch {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

struct HttpSession : std::enable_shared_from_this<HttpSession>
{
	tcp::resolver resolver;
	tcp::socket socket;
	beast::flat_buffer buffer;
	http::request<http::string_body> req;
	http::response<http::string_body> res;
	std::string host;
	std::string port;
	IHttpClient::HttpCallback cb;

	HttpSession(asio::io_context& io, std::string h, std::string p,
		http::request<http::string_body>&& request, IHttpClient::HttpCallback callback)
		: resolver(io), socket(io), req(std::move(request)),
		  host(std::move(h)), port(std::move(p)), cb(std::move(callback))
	{
	}

	void fail(beast::error_code ec, const char* what)
	{
		if (cb) {
			cb(0, std::string(), std::string(what) + ": " + ec.message());
		}
	}

	void start()
	{
		auto self = shared_from_this();
		resolver.async_resolve(host, port, [this, self](beast::error_code ec, tcp::resolver::results_type results) {
			if (ec) {
				fail(ec, "resolve");
				return;
			}
			asio::async_connect(socket, results, [this, self](beast::error_code ec2, tcp::endpoint) {
				if (ec2) {
					fail(ec2, "connect");
					return;
				}
				http::async_write(socket, req, [this, self](beast::error_code ec3, std::size_t) {
					if (ec3) {
						fail(ec3, "write");
						return;
					}
					http::async_read(socket, buffer, res, [this, self](beast::error_code ec4, std::size_t) {
						if (ec4 && ec4 != http::error::end_of_stream) {
							fail(ec4, "read");
							return;
						}
						if (cb) {
							cb(static_cast<int>(res.result_int()), res.body(), std::string());
						}
					});
				});
			});
		});
	}
};

} // namespace

void HttpClient::get(const char* asioContextName, const std::string& host, const std::string& port,
	const std::string& target, HttpCallback callback)
{
	request(false, asioContextName, host, port, target, std::string(), std::move(callback));
}

void HttpClient::post(const char* asioContextName, const std::string& host, const std::string& port,
	const std::string& target, const std::string& body, HttpCallback callback)
{
	request(true, asioContextName, host, port, target, body, std::move(callback));
}

void HttpClient::request(bool isPost, const char* asioContextName, const std::string& host,
	const std::string& port, const std::string& target, const std::string& body, HttpCallback callback)
{
	if (host.empty() || port.empty() || target.empty()) {
		if (callback) {
			callback(0, std::string(), "host/port/target is empty");
		}
		return;
	}

	http::request<http::string_body> req{ http::verb::get, target, 11 };
	if (isPost) {
		req.method(http::verb::post);
		req.body() = body;
	}
	req.set(http::field::host, host + ":" + port);
	req.set(http::field::user_agent, "PlugCompHub/0.2.1");
	req.prepare_payload();

	auto ctx = AsioContextManager::instance().get(asioContextName);
	auto session = std::make_shared<HttpSession>(ctx->io(), host, port, std::move(req), std::move(callback));
	session->start();
}

}
