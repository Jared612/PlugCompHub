#include "httpclient.h"
#include "httplib.h"
#include <thread>

namespace pch {

static void doRequest(const std::string& scheme_host, int port,
                      const std::string& target, const std::string& body,
                      IHttpClient::HttpCallback callback, bool isPost)
{
	httplib::Client cli(scheme_host, port);
	cli.set_connection_timeout(10, 0);
	cli.set_read_timeout(30, 0);

	httplib::Result res;
	if (isPost) {
		res = cli.Post(target, body, "text/plain");
	} else {
		res = cli.Get(target);
	}

	if (res) {
		callback(res->status, res->body, "");
	} else {
		callback(-1, "", httplib::to_string(res.error()));
	}
}

void HttpClient::get(const char* /*asioContextName*/, const std::string& host, const std::string& port,
                     const std::string& target, HttpCallback callback)
{
	int p = std::stoi(port);
	std::thread(doRequest, host, p, target, "", std::move(callback), false).detach();
}

void HttpClient::post(const char* /*asioContextName*/, const std::string& host, const std::string& port,
                      const std::string& target, const std::string& body, HttpCallback callback)
{
	int p = std::stoi(port);
	std::thread(doRequest, host, p, target, body, std::move(callback), true).detach();
}

}
