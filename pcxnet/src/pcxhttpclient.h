#pragma once

#include "ipcxhttpclient.h"

namespace pcx {

class PcxHttpClient : public IPcxHttpClient
{
public:
	PcxHttpClient() = default;
	~PcxHttpClient() override = default;

	void get(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, HttpCallback callback) override;

	void post(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, const std::string& body, HttpCallback callback) override;
};

}
