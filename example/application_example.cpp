#include "core.h"
#include "interface.h"
#include "iapplication.h"
#include "example_common.h"

#include <iostream>
#include <string>
#include <vector>

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::cerr << "initialize failed\n";
		return 1;
	}

	auto* pm = static_cast<pch::IPluginManager*>(pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, PCH_APPLICATION_LIB).c_str()) == nullptr) {
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
