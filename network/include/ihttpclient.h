/**
 * @file ihttpclient.h
 * @brief HTTP 客户端抽象接口（基于异步回调）
 */
#pragma once

#include <functional>
#include <string>

namespace pch {

class IHttpClient
{
public:
	using HttpCallback = std::function<void(int status_code, const std::string& response, const std::string& err_msg)>;

	virtual ~IHttpClient() = default;

	virtual void get(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, HttpCallback callback) = 0;

	virtual void post(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, const std::string& body, HttpCallback callback) = 0;
};

}
