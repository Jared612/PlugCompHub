#include "core.h"
#include "pluginManager.h"
#include "iHttpClient.h"
#include "iWebSocketClient.h"

#include <iostream>
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
	if (pch::api::Initialize(joinPath(dir, "PCH.dll").c_str()) != PCH_SUCCESS) {
		std::cerr << "initialize failed\n";
		return 1;
	}

	auto* pm = static_cast<pch::PluginManager*>(pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, "pchnetwork.dll").c_str()) == nullptr) {
		std::cerr << "load pchnetwork plugin failed\n";
		return 1;
	}

	auto* httpClient = static_cast<pch::IHttpClient*>(pch::api::CreateNamedObject("cpp.pch.httpclient", "example.http.client"));
	auto* wsClient = static_cast<pch::IWebSocketClient*>(pch::api::CreateNamedObject("cpp.pch.websocketclient", "example.ws.client"));
	if (httpClient == nullptr || wsClient == nullptr) {
		std::cerr << "create client objects failed\n";
		pch::api::Terminate();
		return 1;
	}

	std::cout << "net example ok: httpclient=" << (httpClient ? "created" : "null")
	          << " wsclient=" << (wsClient ? "created" : "null") << "\n";

	pch::api::Terminate();
	return 0;
}
