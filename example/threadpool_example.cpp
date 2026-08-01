#include "core.h"
#include "pluginManager.h"
#include "ithreadpool.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

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
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, "pchthreadpool.dll").c_str()) == nullptr) {
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
