#include <pch.h>
#include "example_common.h"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace {

int g_failures = 0;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++g_failures; \
		} \
	} while (0)

struct Dummy : public pch::IMessageHandler
{
	static std::atomic<int> alive;
	static std::mutex m;
	static std::condition_variable cv;
	static bool entered;
	static bool go;

	Dummy() { alive++; }
	~Dummy() { alive--; }

	const pch::IMessage* handleMessage(const pch::IMessage* msg) override
	{
		if (msg && msg->getCode() == pch::CommandMessage) {
			{
				std::lock_guard<std::mutex> lk(m);
				entered = true;
			}
			cv.notify_all();
			std::unique_lock<std::mutex> lk(m);
			cv.wait(lk, [] { return go; });
		}
		return nullptr;
	}
};

std::atomic<int> Dummy::alive{ 0 };
std::mutex Dummy::m;
std::condition_variable Dummy::cv;
bool Dummy::entered = false;
bool Dummy::go = false;

} // namespace

PCH_REGISTER_COMPONENT(Dummy, "test.core.dummy")

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::fprintf(stderr, "Initialize failed\n");
		return 1;
	}

	auto* cm = static_cast<pch::IComponentManager*>(pch::api::FindObject(PCH_DEFAULT_COMPONENTMANAGER));
	CHECK(cm != nullptr);
	CHECK(cm->registerComponent(const_cast<pch::ComponentInfo*>(&pch::ComponentTmpl<Dummy>::componentInfo)) == PCH_SUCCESS);

	void* obj = pch::api::CreateNamedObject("test.core.dummy", "test.dummy");
	CHECK(obj != nullptr);
	CHECK(Dummy::alive.load() == 1);

	auto* mc = static_cast<pch::IMessageCenter*>(pch::api::FindObject(PCH_DEFAULT_MESSAGECENTER));
	CHECK(mc != nullptr);

	// 分发线程进入 handler 后阻塞，模拟"消息正在被处理"
	std::thread sender([&]() {
		pch::IMessage* msg = mc->allocMessage(pch::CommandMessage, "test", 0, nullptr, pch::CommandMessage);
		mc->sendMessage("test.dummy", msg, nullptr);
	});

	{
		std::unique_lock<std::mutex> lk(Dummy::m);
		Dummy::cv.wait(lk, [] { return Dummy::entered; });
	}

	// 对象正在被分发：DeleteObject 应立即成功、对象立即不可见、实际销毁延后
	CHECK(pch::api::DeleteObject(obj) == PCH_SUCCESS);
	CHECK(pch::api::FindObject("test.dummy") == nullptr);
	CHECK(Dummy::alive.load() == 1); // 延迟销毁：对象仍存活

	{
		std::lock_guard<std::mutex> lk(Dummy::m);
		Dummy::go = true;
	}
	Dummy::cv.notify_all();
	sender.join();

	// 分发结束，releaseObject 完成实际销毁
	CHECK(Dummy::alive.load() == 0);

	pch::api::Terminate();

	if (g_failures != 0) {
		std::fprintf(stderr, "object_lifetime_test: %d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("object lifetime test ok\n");
	return 0;
}
