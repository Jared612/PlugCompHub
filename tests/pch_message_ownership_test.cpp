#include <pch.h>
#include "example_common.h"

#include <cstdio>
#include <cstring>

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

	auto* mc = static_cast<pch::IMessageCenter*>(pch::api::FindObject(PCH_DEFAULT_MESSAGECENTER));
	CHECK(mc != nullptr);

	// sendMessage 到不存在的目标：返回错误，消息由中心释放并置空
	pch::IMessage* msg = mc->allocMessage(pch::CommandMessage, "test", 4, (void*)"abcd", pch::CommandMessage);
	CHECK(msg != nullptr);
	if (msg) {
		pch::ErrorCode ec = mc->sendMessage("no.such.object", msg, nullptr);
		CHECK(ec == PCH_OBJECT_NOTFOUND);
		CHECK(msg == nullptr);
	}

	// postMessage 到不存在的目标：同样由中心释放并置空
	pch::IMessage* msg2 = mc->allocMessage(pch::CommandMessage, "test", 0, nullptr, pch::CommandMessage);
	CHECK(msg2 != nullptr);
	if (msg2) {
		CHECK(mc->postMessage("no.such.object", msg2) == PCH_OBJECT_NOTFOUND);
		CHECK(msg2 == nullptr);
	}

	// 组播 + BreakOnError：遇到缺失目标即停止，消息仍被释放
	pch::IMessage* msg3 = mc->allocMessage(pch::CommandMessage, "test", 0, nullptr, pch::CommandMessage);
	CHECK(msg3 != nullptr);
	if (msg3) {
		const char* group[] = { PCH_DEFAULT_LOGGERMANAGER, "no.such.object" };
		pch::ErrorCode ec = mc->multicastLocalMessage(group, 2, msg3, pch::BreakOnError);
		CHECK(ec == PCH_OBJECT_NOTFOUND);
		CHECK(msg3 == nullptr);
	}

	// 负数 count：参数错误，消息仍归调用方
	pch::IMessage* msg4 = mc->allocMessage(pch::CommandMessage, "test", 0, nullptr, pch::CommandMessage);
	CHECK(msg4 != nullptr);
	if (msg4) {
		CHECK(mc->multicastLocalMessage(nullptr, -1, msg4) == PCH_PARAM_INVALID);
		CHECK(msg4 != nullptr);
		CHECK(mc->freeMessage(msg4) == PCH_SUCCESS);
	}

	// 广播：目标均忽略该消息，广播结束后消息被释放
	pch::IMessage* msg5 = mc->allocMessage(pch::CommandMessage, "test", 0, nullptr, pch::CommandMessage);
	CHECK(msg5 != nullptr);
	if (msg5) {
		CHECK(mc->broadcastLocalMessage(msg5) == PCH_SUCCESS);
		CHECK(msg5 == nullptr);
	}

	pch::api::Terminate();

	if (g_failures != 0) {
		std::fprintf(stderr, "message_ownership_test: %d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("message ownership test ok\n");
	return 0;
}
