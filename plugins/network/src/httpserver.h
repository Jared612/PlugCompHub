/**
 * @file httpserver.h
 * @brief IHttpServer 的 Boost.Beast 异步实现
 */
#pragma once

#include "network/ihttpserver.h"

#include <memory>

namespace pch {

struct HttpListener;

class HttpServer : public IHttpServer
{
public:
	HttpServer() = default;
	virtual ~HttpServer() = default;

	bool start(const char* asioContextName, const std::string& host, const std::string& port, RequestHandler handler) override;
	void stop() override;

private:
	std::shared_ptr<HttpListener> _listener;
};

}
