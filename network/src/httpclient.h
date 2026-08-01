#pragma once

#include "iHttpClient.h"

namespace pch {

class HttpClient : public IHttpClient
{
public:
	HttpClient() = default;
	virtual ~HttpClient() = default;

	void get(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, HttpCallback callback) override;

	void post(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, const std::string& body, HttpCallback callback) override;
};

}
