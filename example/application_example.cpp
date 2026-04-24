#include "pcx.h"
#include "pluginManager.h"
#include "ipcxapplication.h"

#include <iostream>
#include <string>
#include <vector>

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
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, "pcxapplication.dll").c_str()) == nullptr) {
		std::cerr << "load pcxapplication plugin failed\n";
		return 1;
	}

	auto* app = static_cast<pcx::IPcxApplication*>(pcx::api::CreateNamedObject("cpp.pcx.application", "example.app"));
	if (app == nullptr) {
		std::cerr << "create application object failed\n";
		return 1;
	}

	std::vector<std::string> badPlugins, badObjects;
	app->getLoadFailedPluginsInfo(badPlugins, badObjects);
	std::cout << "application example ok, badPlugins=" << badPlugins.size()
	          << ", badObjects=" << badObjects.size() << "\n";

	pcx::api::Terminate();
	return 0;
}
