#include "pcxhttpserver.h"

#include "netcommon.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <chrono>
#include <thread>

namespace pcx {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

class PcxHttpServer::HttpRequestView : public IHttpRequest
{
public:
	explicit HttpRequestView(const http::request<http::string_body>& req)
		: _req(req)
	{
	}

	const char* getMethod() const override
	{
		_method = std::string(_req.method_string());
		return _method.c_str();
	}

	const char* getPath() const override
	{
		const std::string target = std::string(_req.target());
		const std::size_t q = target.find('?');
		_path = (q == std::string::npos) ? target : target.substr(0, q);
		return _path.c_str();
	}

	const char* getTarget() const override
	{
		_target = std::string(_req.target());
		return _target.c_str();
	}

	const char* getHeader(const char* name) const override
	{
		if (name == nullptr) {
			return nullptr;
		}
		auto it = _req.find(name);
		if (it == _req.end()) {
			return nullptr;
		}
		_header = std::string(it->value());
		return _header.c_str();
	}

	const char* getBody() const override
	{
		return _req.body().c_str();
	}

	size_t getBodySize() const override
	{
		return _req.body().size();
	}

private:
	const http::request<http::string_body>& _req;
	mutable std::string _method;
	mutable std::string _path;
	mutable std::string _target;
	mutable std::string _header;
};

class PcxHttpServer::HttpResponseBuilder : public IHttpResponse
{
public:
	HttpResponseBuilder()
		: _response(http::status::ok, 11)
	{
		_response.set(http::field::server, BOOST_BEAST_VERSION_STRING);
		_response.set(http::field::content_type, "text/plain");
	}

	void setStatus(unsigned code) override
	{
		_response.result(static_cast<http::status>(code));
	}

	void setHeader(const char* name, const char* value) override
	{
		if (name == nullptr || value == nullptr) {
			return;
		}
		_response.set(name, value);
	}

	void setBody(const char* data, size_t size) override
	{
		if (data == nullptr) {
			_response.body().clear();
		}
		else {
			_response.body().assign(data, size);
		}
		_response.prepare_payload();
	}

	void setBody(const char* text) override
	{
		if (text == nullptr) {
			_response.body().clear();
		}
		else {
			_response.body() = text;
		}
		_response.prepare_payload();
	}

	http::response<http::string_body> build() const
	{
		return _response;
	}

private:
	http::response<http::string_body> _response;
};

bool PcxHttpServer::start(const char* address, unsigned short port, const char* asioContextName)
{
	// 不持有独立 io_context：从已注册的 IPcxAsioContext 取 boost::asio::io_context，与 WebSocket/HTTP 客户端共用事件循环
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
	tcp::endpoint endpoint(ip, port);
	_acceptor->open(endpoint.protocol(), ec);
	if (ec) {
		return false;
	}
	_acceptor->set_option(net::socket_base::reuse_address(true), ec);
	if (ec) {
		return false;
	}
	_acceptor->bind(endpoint, ec);
	if (ec) {
		return false;
	}
	_acceptor->listen(net::socket_base::max_listen_connections, ec);
	if (ec) {
		return false;
	}
	_acceptor->non_blocking(true, ec);
	if (ec) {
		return false;
	}

	_bindAddress = address;
	_port = port;
	_asioContextName = asioContextName;
	_running = true;
	_acceptThread = std::thread([this]() { acceptLoop(); });
	return true;
}

void PcxHttpServer::stop()
{
	if (_acceptor) {
		boost::system::error_code ec;
		_acceptor->cancel(ec);
		_acceptor->close(ec);
	}
	if (_acceptThread.joinable()) {
		_acceptThread.join();
	}
	_acceptor.reset();
	_running = false;
}

void PcxHttpServer::addRoute(const char* method, const char* path, HttpRouteHandler handler)
{
	if (method == nullptr || path == nullptr) {
		return;
	}
	std::lock_guard<std::mutex> lk(_mutex);
	_routes[routeKey(method, path)] = std::move(handler);
}

bool PcxHttpServer::isRunning() const
{
	return _running;
}

std::string PcxHttpServer::routeKey(const char* method, const char* path)
{
	std::string key = method ? method : "";
	key.push_back(':');
	key += (path ? path : "");
	return key;
}

HttpRouteHandler PcxHttpServer::findRoute(const std::string& method, const std::string& path) const
{
	std::lock_guard<std::mutex> lk(_mutex);
	const auto it = _routes.find(routeKey(method.c_str(), path.c_str()));
	if (it == _routes.end()) {
		return {};
	}
	return it->second;
}

void PcxHttpServer::acceptLoop()
{
	while (_running && _acceptor) {
		boost::system::error_code ec;
		tcp::socket socket(*_ioContext);
		_acceptor->accept(socket, ec);
		if (ec) {
			if (ec == net::error::would_block || ec == net::error::try_again) {
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				continue;
			}
			if (ec == net::error::operation_aborted || ec == net::error::bad_descriptor) {
				break;
			}
			if (_running) {
				continue;
			}
			break;
		}
		std::thread(&PcxHttpServer::handleClient, this, std::move(socket)).detach();
	}
}

void PcxHttpServer::handleClient(tcp::socket socket)
{
	try {
		beast::flat_buffer buffer;
		http::request<http::string_body> req;
		http::read(socket, buffer, req);

		const std::string target = std::string(req.target());
		const std::size_t q = target.find('?');
		const std::string path = (q == std::string::npos) ? target : target.substr(0, q);
		const std::string method = std::string(req.method_string());

		HttpResponseBuilder builder;
		HttpRequestView view(req);
		const auto handler = findRoute(method, path);
		if (handler) {
			handler(view, builder);
		}
		else {
			builder.setStatus(404);
			builder.setBody("route not found");
		}

		auto res = builder.build();
		res.keep_alive(false);
		http::write(socket, res);

		boost::system::error_code ec;
		socket.shutdown(tcp::socket::shutdown_send, ec);
	}
	catch (...) {
	}
}

}
