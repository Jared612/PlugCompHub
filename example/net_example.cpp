#include "pcx.h"
#include "pluginManager.h"
#include "ipcxasiocontext.h"
#include "ipcxhttpclient.h"
#include "ipcxhttpserver.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <Windows.h>
static std::string exeDir()
{
	char buf[MAX_PATH];
	DWORD n = GetModuleFileNameA(nullptr, buf, static_cast<DWORD>(sizeof(buf)));
	if (n == 0 || n >= sizeof(buf)) return ".";
	std::string p(buf, n);
	auto pos = p.find_last_of("\\/");
	return (pos == std::string::npos) ? "." : p.substr(0, pos);
}
static std::string joinPath(const std::string& d, const char* f) { return d + "\\" + f; }
#else
static std::string exeDir() { return "."; }
static std::string joinPath(const std::string& d, const char* f) { return d + "/" + f; }
#endif

int main()
{
	const std::string dir = exeDir();
	if (pcx::api::Initialize(joinPath(dir, "pcx.dll").c_str()) != PCX_SUCCESS) {
		std::cerr << "initialize failed\n";
		return 1;
	}

	auto* pm = static_cast<pcx::PluginManager*>(pcx::api::FindObject(PCX_DEFAULT_PLUGINMANAGER));
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, "pcxnet.dll").c_str()) == nullptr) {
		std::cerr << "load pcxnet plugin failed\n";
		return 1;
	}

	auto* asio = static_cast<pcx::IPcxAsioContext*>(pcx::api::CreateNamedObject("cpp.pcx.asiocontext", "example.asio"));
	auto* server = static_cast<pcx::IPcxHttpServer*>(pcx::api::CreateNamedObject("cpp.pcx.httpserver", "example.http.server"));
	auto* client = static_cast<pcx::IPcxHttpClient*>(pcx::api::CreateNamedObject("cpp.pcx.httpclient", "example.http.client"));
	if (asio == nullptr || server == nullptr || client == nullptr) {
		std::cerr << "create net objects failed\n";
		return 1;
	}

	asio->start(2);
	server->addRoute("GET", "/ping", [](const pcx::IHttpRequest&, pcx::IHttpResponse& rsp) {
		rsp.setStatus(200);
		rsp.setBody("pong");
	});
	if (!server->start("127.0.0.1", 19082, "example.asio")) {
		std::cerr << "start server failed\n";
		return 1;
	}

	std::mutex m;
	std::condition_variable cv;
	bool done = false;
	int code = 0;
	std::string body;
	std::string err;

	client->get("example.asio", "127.0.0.1", "19082", "/ping",
		[&](int status, const std::string& rspBody, const std::string& e) {
			std::lock_guard<std::mutex> lk(m);
			done = true;
			code = status;
			body = rspBody;
			err = e;
			cv.notify_one();
		});

	{
		std::unique_lock<std::mutex> lk(m);
		cv.wait_for(lk, std::chrono::seconds(5), [&]() { return done; });
	}

	server->stop();
	asio->stop();
	pcx::api::Terminate();

	if (!done || code != 200 || body != "pong" || !err.empty()) {
		std::cerr << "net example failed, done=" << done << ", code=" << code
		          << ", body=" << body << ", err=" << err << "\n";
		return 1;
	}

	std::cout << "net example ok\n";
	return 0;
}
