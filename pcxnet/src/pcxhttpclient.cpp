#include "pcxhttpclient.h"

#include "netcommon.h"

#include <boost/asio.hpp>
#include <boost/asio/connect.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <memory>

namespace pcx {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

void PcxHttpClient::get(const char* asioContextName, const std::string& host, const std::string& port,
	const std::string& target, HttpCallback callback)
{
	auto* io = resolveIoContext(asioContextName);
	if (callback == nullptr) {
		return;
	}
	if (io == nullptr) {
		callback(0, "", "asio context not found");
		return;
	}

	net::post(*io, [io, host, port, target, callback = std::move(callback)]() mutable {
		try {
			tcp::resolver resolver(*io);
			beast::tcp_stream stream(*io);

			auto const results = resolver.resolve(host, port);
			stream.connect(results);

			http::request<http::string_body> req{ http::verb::get, target, 11 };
			req.set(http::field::host, host);
			req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

			http::write(stream, req);

			beast::flat_buffer buffer;
			http::response<http::string_body> res;
			http::read(stream, buffer, res);

			beast::error_code ec;
			stream.socket().shutdown(tcp::socket::shutdown_both, ec);

			callback(static_cast<int>(res.result_int()), res.body(), "");
		}
		catch (const std::exception& ex) {
			callback(0, "", ex.what());
		}
	});
}

void PcxHttpClient::post(const char* asioContextName, const std::string& host, const std::string& port,
	const std::string& target, const std::string& body, HttpCallback callback)
{
	auto* io = resolveIoContext(asioContextName);
	if (callback == nullptr) {
		return;
	}
	if (io == nullptr) {
		callback(0, "", "asio context not found");
		return;
	}

	net::post(*io, [io, host, port, target, body, callback = std::move(callback)]() mutable {
		try {
			tcp::resolver resolver(*io);
			beast::tcp_stream stream(*io);

			auto const results = resolver.resolve(host, port);
			stream.connect(results);

			http::request<http::string_body> req{ http::verb::post, target, 11 };
			req.set(http::field::host, host);
			req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
			req.set(http::field::content_type, "text/plain");
			req.body() = body;
			req.prepare_payload();

			http::write(stream, req);

			beast::flat_buffer buffer;
			http::response<http::string_body> res;
			http::read(stream, buffer, res);

			beast::error_code ec;
			stream.socket().shutdown(tcp::socket::shutdown_both, ec);

			callback(static_cast<int>(res.result_int()), res.body(), "");
		}
		catch (const std::exception& ex) {
			callback(0, "", ex.what());
		}
	});
}

}
