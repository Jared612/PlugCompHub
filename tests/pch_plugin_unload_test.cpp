#include <pch.h>
#include "example_common.h"
#include "threadpool/ithreadpool.h"

#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

} // namespace

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::fprintf(stderr, "Initialize failed\n");
		return 1;
	}

	auto* pm = static_cast<pch::IPluginManager*>(pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
	CHECK(pm != nullptr);

	pch::IPlugin* plugin = pm->loadPlugin(joinPath(dir, PCH_THREADPOOL_LIB).c_str());
	CHECK(plugin != nullptr);
	if (plugin == nullptr) {
		pch::api::Terminate();
		return 1;
	}

	auto* tp = static_cast<pch::IThreadPool*>(
		pch::api::CreateNamedObject("cpp.pch.threadpool", "test.unload.tp"));
	CHECK(tp != nullptr);

	// 有活对象：卸载必须被拒绝（避免 DLL 卸载后 deletor 失效）
	CHECK(pm->unloadPlugin(plugin) == PCH_NOTALLOW);

	// 删除对象后可正常卸载
	CHECK(pch::api::DeleteObject(tp) == PCH_SUCCESS);
	CHECK(pm->unloadPlugin(plugin) == PCH_SUCCESS);

	// 卸载后同名插件可重新加载
	pch::IPlugin* p2 = pm->loadPlugin(joinPath(dir, PCH_THREADPOOL_LIB).c_str());
	CHECK(p2 != nullptr);
	if (p2) {
		CHECK(pm->unloadPlugin(p2) == PCH_SUCCESS);
	}

	pch::api::Terminate();

	if (g_failures != 0) {
		std::fprintf(stderr, "plugin_unload_test: %d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("plugin unload test ok\n");
	return 0;
}

