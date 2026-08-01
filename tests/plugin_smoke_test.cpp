#include "core/core.h"
#include "core/componentinfo.h"
#include "core/interface.h"
#include "threadpool/ithreadpool.h"
#include "example_common.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::fprintf(stderr, "Initialize failed\n");
		return 1;
	}
	auto* pm = static_cast<pch::IPluginManager*>(pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
	pch::IPlugin* plugin = (pm != nullptr)
		? pm->loadPlugin(joinPath(dir, PCH_THREADPOOL_LIB).c_str())
		: nullptr;
	if (plugin == nullptr) {
		std::fprintf(stderr, "load pchthreadpool plugin failed\n");
		pch::api::Terminate();
		return 1;
	}
	auto* tp = static_cast<pch::IThreadPool*>(pch::api::CreateNamedObject("cpp.pch.threadpool", "test.tp"));
	if (tp == nullptr) {
		std::fprintf(stderr, "create threadpool object failed\n");
		pch::api::Terminate();
		return 1;
	}

	std::atomic<int> done{ 0 };
	tp->start(2);
	for (int i = 0; i < 4; ++i) {
		tp->exec([&done]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			++done;
		});
	}
	if (!tp->waitForAllDone(3000)) {
		std::fprintf(stderr, "waitForAllDone timeout\n");
		tp->stop();
		pch::api::Terminate();
		return 1;
	}
	tp->stop();
	if (done.load() != 4) {
		std::fprintf(stderr, "done=%d expected 4\n", done.load());
		pch::api::Terminate();
		return 1;
	}

	pch::api::Terminate();
	std::printf("plugin smoke ok: done=%d\n", done.load());
	return 0;
}
