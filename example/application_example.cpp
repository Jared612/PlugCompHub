#include "core.h"
#include "pluginManager.h"
#include "iApplication.h"

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
	if (pch::api::Initialize(joinPath(dir, "PCH.dll").c_str()) != PCH_SUCCESS) {
		std::cerr << "initialize failed\n";
		return 1;
	}

	auto* pm = static_cast<pch::PluginManager*>(pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, "pchapplication.dll").c_str()) == nullptr) {
		std::cerr << "load pchapplication plugin failed\n";
		return 1;
	}

	auto* app = static_cast<pch::IApplication*>(pch::api::CreateNamedObject("cpp.pch.application", "example.app"));
	if (app == nullptr) {
		std::cerr << "create application object failed\n";
		return 1;
	}

	std::vector<std::string> badPlugins, badObjects;
	app->getLoadFailedPluginsInfo(badPlugins, badObjects);
	std::cout << "application example ok, badPlugins=" << badPlugins.size()
	          << ", badObjects=" << badObjects.size() << "\n";

	pch::api::Terminate();
	return 0;
}
