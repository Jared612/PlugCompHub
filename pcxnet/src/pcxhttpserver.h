#pragma once

#include "ipcxhttpserver.h"

#include <boost/asio.hpp>
#include <boost/beast/http.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace pcx {

class PcxHttpServer : public IPcxHttpServer
{
public:
	PcxHttpServer() = default;
	~PcxHttpServer() override = default;

	bool start(const char* address, unsigned short port, const char* asioContextName) override;
	void stop() override;
	void addRoute(const char* method, const char* path, HttpRouteHandler handler) override;
	bool isRunning() const override;

private:
	static std::string routeKey(const char* method, const char* path);
	class HttpRequestView;
	class HttpResponseBuilder;

	void acceptLoop();
	void handleClient(boost::asio::ip::tcp::socket socket);
	HttpRouteHandler findRoute(const std::string& method, const std::string& path) const;

	std::atomic<bool> _running{ false };
	std::string _bindAddress;
	unsigned short _port{ 0 };
	std::string _asioContextName;
	boost::asio::io_context* _ioContext{ nullptr };

	mutable std::mutex _mutex;
	std::unordered_map<std::string, HttpRouteHandler> _routes;

	std::unique_ptr<boost::asio::ip::tcp::acceptor> _acceptor;
	std::thread _acceptThread;
};

}
