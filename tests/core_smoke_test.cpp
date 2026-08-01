#include "core.h"
#include "componentinfo.h"
#include "interface.h"
#include "example_common.h"

#include <cstdio>

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::fprintf(stderr, "Initialize failed\n");
		return 1;
	}
	if (pch::api::FindObject(PCH_DEFAULT_OBJECTMANAGER) == nullptr
		|| pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER) == nullptr
		|| pch::api::FindObject(PCH_DEFAULT_COMPONENTMANAGER) == nullptr) {
		std::fprintf(stderr, "core managers not found\n");
		pch::api::Terminate();
		return 1;
	}
	if (pch::api::Terminate() != PCH_SUCCESS) {
		std::fprintf(stderr, "Terminate failed\n");
		return 1;
	}
	std::printf("core smoke ok\n");
	return 0;
}
