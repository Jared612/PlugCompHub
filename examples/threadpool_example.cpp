#include "core.h"
#include "interface.h"
#include "ithreadpool.h"
#include "example_common.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::cerr << "initialize failed\n";
		return 1;
	}

	auto* pm = static_cast<pch::IPluginManager*>(pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, PCH_THREADPOOL_LIB).c_str()) == nullptr) {
		std::cerr << "load pchthreadpool plugin failed\n";
		return 1;
	}

	auto* tp = static_cast<pch::IThreadPool*>(pch::api::CreateNamedObject("cpp.pch.threadpool", "example.tp"));
	if (tp == nullptr) {
		std::cerr << "create threadpool object failed\n";
		return 1;
	}

	std::atomic<int> done{ 0 };
	tp->start(2);
	for (int i = 0; i < 5; ++i) {
		tp->exec([&done]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			++done;
		});
	}
	if (!tp->waitForAllDone(3000)) {
		std::cerr << "wait timeout\n";
		return 1;
	}
	tp->stop();

	std::cout << "threadpool example ok, done=" << done.load() << "\n";
	pch::api::Terminate();
	return 0;
}
