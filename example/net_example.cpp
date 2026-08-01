#include "core.h"
#include "interface.h"
#include "ihttpclient.h"
#include "iwebsocketclient.h"
#include "example_common.h"

#include <iostream>
#include <string>

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::cerr << "initialize failed\n";
		return 1;
	}

	auto* pm = static_cast<pch::IPluginManager*>(pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, PCH_NETWORK_LIB).c_str()) == nullptr) {
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
