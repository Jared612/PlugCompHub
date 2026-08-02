#include <pch.h>
#include "example_common.h"

#include <cstdio>
#include <string>
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

class CaptureWrite : public pch::ILoggerWrite
{
public:
	std::vector<std::string> lines;

	void writeLog(pch::LogLevel /*level*/, const char* logText) override
	{
		lines.push_back(logText);
	}

	void flush() override {}
};

} // namespace

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::fprintf(stderr, "Initialize failed\n");
		return 1;
	}

	auto* lm = static_cast<pch::ILoggerManager*>(pch::api::FindObject(PCH_DEFAULT_LOGGERMANAGER));
	CHECK(lm != nullptr);

	// 回归：命名日志器 "test.A" 只有后端、级别未显式设置；父级设置 Error 级别后，
	// getLogger("test.A.B") 必须继承 Error 级别（旧实现会提前 break 落到默认 Trace）
	CaptureWrite writer;
	CHECK(lm->setLogger("test.A", &writer, nullptr) == true);
	CHECK(lm->setLoggerLevel("test.A", pch::LogLevel::Error) == true);

	pch::ILogger* logger = lm->getLogger("test.A.B");
	CHECK(logger != nullptr);
	if (logger) {
		logger->info("filtered-info");
		logger->warn("filtered-warn");
		logger->error("shown-error");
	}
	CHECK(writer.lines.size() == 1);
	if (writer.lines.size() == 1) {
		CHECK(writer.lines[0] == "shown-error");
	}

	// SystemReady 之后默认日志器不应被锁死：addDefaultLogger 仍应成功挂载新后端
	auto* mc = static_cast<pch::IMessageCenter*>(pch::api::FindObject(PCH_DEFAULT_MESSAGECENTER));
	CHECK(mc != nullptr);
	if (mc) {
		pch::IMessage* ready = mc->allocMessage(pch::SystemReady, nullptr, 0, nullptr, pch::SystemMessage);
		CHECK(ready != nullptr);
		if (ready) {
			CHECK(mc->sendMessage(PCH_DEFAULT_LOGGERMANAGER, ready, nullptr) == PCH_SUCCESS);
			CHECK(ready == nullptr); // 消息中心负责释放
		}
	}

	CaptureWrite defaultWriter;
	CHECK(lm->addDefaultLogger(&defaultWriter, nullptr) == true);
	if (lm->getDefaultLogger()) {
		lm->getDefaultLogger()->info("default-log");
	}
	CHECK(!defaultWriter.lines.empty());

	pch::api::Terminate();

	if (g_failures != 0) {
		std::fprintf(stderr, "logger_hierarchy_test: %d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("logger hierarchy test ok\n");
	return 0;
}
