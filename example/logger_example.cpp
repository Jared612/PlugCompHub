#include "core.h"
#include "ifilelogger.h"
#include "interface.h"
#include "example_common.h"

#include <iostream>
#include <string>

int main()
{
	const std::string dir = exeDir();
	if (pch::api::Initialize(joinPath(dir, PCH_CORE_LIB).c_str()) != PCH_SUCCESS) {
		std::cerr << "initialize failed\n";
		return 1;
	}

	auto* pm = static_cast<pch::IPluginManager*>(pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER));
	if (pm == nullptr || pm->loadPlugin(joinPath(dir, PCH_LOGGER_LIB).c_str()) == nullptr) {
		std::cerr << "load pchlogger plugin failed\n";
		return 1;
	}

	auto* logger = static_cast<pch::IFileLogger*>(
		pch::api::CreateNamedObject("cpp.pch.filelogger", "example.filelogger"));
	if (logger == nullptr) {
		std::cerr << "create file logger failed\n";
		return 1;
	}

	if (!logger->open((dir + "\\example.log").c_str(), pch::LogLevel::Information)) {
		std::cerr << "open log file failed\n";
		return 1;
	}

	// 写入示例日志 — 直接从日志器对象使用 ILogger 接口
	logger->writeLog(pch::LogLevel::Information, "Logger example started");
	logger->writeLog(pch::LogLevel::Warning, "This is a warning");
	logger->writeLog(pch::LogLevel::Error, "This is an error");
	logger->writeLog(pch::LogLevel::Debug, "This debug message should be filtered out");

	logger->flush();
	logger->close();

	std::cout << "logger example ok, check example.log\n";
	pch::api::Terminate();
	return 0;
}
