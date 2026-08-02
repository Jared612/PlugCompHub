#include <pch.h>
#include "example_common.h"

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

struct Worker : public pch::IMessageHandler
{
	static std::atomic<int> alive;
	static std::atomic<long long> handled;

	Worker() { alive++; }
	~Worker() { alive--; }

	const pch::IMessage* handleMessage(const pch::IMessage* /*msg*/) override
	{
		handled++;
		return nullptr;
	}
};

std::atomic<int> Worker::alive{ 0 };
std::atomic<long long> Worker::handled{ 0 };

} // namespace

PCH_REGISTER_COMPONENT(Worker, "test.core.worker")

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::fprintf(stderr, "Initialize failed\n");
		return 1;
	}

	auto* cm = static_cast<pch::IComponentManager*>(pch::api::FindObject(PCH_DEFAULT_COMPONENTMANAGER));
	CHECK(cm != nullptr);
	CHECK(cm->registerComponent(const_cast<pch::ComponentInfo*>(&pch::ComponentTmpl<Worker>::componentInfo)) == PCH_SUCCESS);

	void* hub = pch::api::CreateNamedObject("test.core.worker", "concurrency.hub");
	CHECK(hub != nullptr);

	auto* mc = static_cast<pch::IMessageCenter*>(pch::api::FindObject(PCH_DEFAULT_MESSAGECENTER));
	CHECK(mc != nullptr);

	constexpr int kThreads = 4;
	constexpr int kIters = 200;
	std::vector<std::thread> threads;
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([mc, kIters]() {
			for (int i = 0; i < kIters; ++i) {
				// 并发创建/删除匿名对象，并向稳定的 hub 对象同步发消息
				void* obj = pch::api::CreateObject("test.core.worker");
				if (obj == nullptr) {
					continue;
				}
				pch::IMessage* msg = mc->allocMessage(pch::CommandMessage, "test", 0, nullptr, pch::CommandMessage);
				if (msg) {
					mc->sendMessage("concurrency.hub", msg, nullptr);
				}
				pch::api::DeleteObject(obj);
			}
		});
	}
	for (auto& th : threads) {
		th.join();
	}

	CHECK(Worker::handled.load() == static_cast<long long>(kThreads) * kIters);
	CHECK(Worker::alive.load() == 1); // 只剩 hub
	CHECK(pch::api::DeleteObject(hub) == PCH_SUCCESS);
	CHECK(Worker::alive.load() == 0);

	pch::api::Terminate();

	if (g_failures != 0) {
		std::fprintf(stderr, "concurrency_test: %d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("concurrency test ok\n");
	return 0;
}
