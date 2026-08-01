#include "filelogger.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <cstring>
#include <cstdlib>
#include "core.h"
#include "interface.h"

PCH_BEGIN_NAMESPACE

FileLogger::FileLogger() = default;

FileLogger::~FileLogger()
{
	close();
}

static spdlog::level::level_enum toSpdlogLevel(LogLevel level)
{
	switch (level) {
	case LogLevel::Trace:       return spdlog::level::trace;
	case LogLevel::Debug:       return spdlog::level::debug;
	case LogLevel::Information: return spdlog::level::info;
	case LogLevel::Warning:     return spdlog::level::warn;
	case LogLevel::Error:       return spdlog::level::err;
	case LogLevel::Fatal:       return spdlog::level::critical;
	default:                    return spdlog::level::off;
	}
}

bool FileLogger::open(const char* filePath, LogLevel minLevel)
{
	close();

	try {
		auto sink  = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath, true);
		auto* impl = new std::shared_ptr<spdlog::logger>(std::make_shared<spdlog::logger>("file", sink));
		(*impl)->set_level(toSpdlogLevel(minLevel));
		(*impl)->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
		(*impl)->flush_on(spdlog::level::err);
		_loggerImpl = impl;
		_minLevel   = minLevel;
		_isOpen     = true;
		return true;
	} catch (const spdlog::spdlog_ex&) {
		_isOpen = false;
		return false;
	}
}

void FileLogger::setMinLevel(LogLevel level)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_minLevel = level;
	if (_loggerImpl) {
		auto* impl = static_cast<std::shared_ptr<spdlog::logger>*>(_loggerImpl);
		(*impl)->set_level(toSpdlogLevel(level));
	}
}

void FileLogger::close()
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (_loggerImpl) {
		delete static_cast<std::shared_ptr<spdlog::logger>*>(_loggerImpl);
		_loggerImpl = nullptr;
	}
	_isOpen = false;
}

bool FileLogger::isOpen() const
{
	return _isOpen;
}

void FileLogger::writeLog(pch::LogLevel level, const char* logText)
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (!_loggerImpl) return;

	auto* impl = static_cast<std::shared_ptr<spdlog::logger>*>(_loggerImpl);
	(*impl)->log(toSpdlogLevel(level), "{}", logText);
}

void FileLogger::flush()
{
	std::lock_guard<std::mutex> lock(_mutex);
	if (_loggerImpl) {
		auto* impl = static_cast<std::shared_ptr<spdlog::logger>*>(_loggerImpl);
		(*impl)->flush();
	}
}

static LogLevel parseLevelString(const char* s)
{
	if (s == nullptr) return LogLevel::Information;
	if (std::strcmp(s, "Trace") == 0)       return LogLevel::Trace;
	if (std::strcmp(s, "Debug") == 0)       return LogLevel::Debug;
	if (std::strcmp(s, "Information") == 0) return LogLevel::Information;
	if (std::strcmp(s, "Warning") == 0)     return LogLevel::Warning;
	if (std::strcmp(s, "Error") == 0)       return LogLevel::Error;
	if (std::strcmp(s, "Fatal") == 0)       return LogLevel::Fatal;
	return LogLevel::Information;
}

const IMessage* FileLogger::handleMessage(const IMessage* msg)
{
	if (msg == nullptr) return nullptr;

	// 只处理系统对象初始化消息
	if (msg->getType() != SystemObjectInit) return nullptr;
	if (msg->getCode() != 0) return nullptr;

	const auto* raw = msg->getData();
	if (raw == nullptr) return nullptr;

	const char* data = reinterpret_cast<const char*>(raw);
	// 格式: "path=./logs/app.log,level=Information,logname=threadpool"
	const char* pathEq = std::strstr(data, "path=");
	const char* levelEq = std::strstr(data, "level=");
	const char* lognameEq = std::strstr(data, "logname=");
	if (pathEq == nullptr) {
		return nullptr;
	}

	pathEq += 5;  // 跳过 "path="
	char path[512] = { 0 };
	int i = 0;
	while (*pathEq && *pathEq != ',' && i < 511) {
		path[i++] = *pathEq++;
	}
	path[i] = '\0';

	LogLevel lv = LogLevel::Information;
	if (levelEq) {
		levelEq += 6;  // 跳过 "level="
		char level[64] = { 0 };
		int j = 0;
		while (*levelEq && *levelEq != ',' && j < 63) {
			level[j++] = *levelEq++;
		}
		level[j] = '\0';
		lv = parseLevelString(level);
	}

	if (lognameEq) {
		lognameEq += 8;  // 跳过 "logname="
		char name[128] = { 0 };
		int k = 0;
		while (*lognameEq && *lognameEq != ',' && k < 127) {
			name[k++] = *lognameEq++;
		}
		name[k] = '\0';
		_logName = name;
	}

	open(path, lv);

	// 如果指定了命名日志器，将自己注册到 LoggerManager 的对应条目中
	if (!_logName.empty()) {
		ILoggerManager* lm = (ILoggerManager*)pch::api::FindObject(PCH_DEFAULT_LOGGERMANAGER);
		if (lm) {
			lm->setLogger(_logName.c_str(), this, nullptr);
		}
	}
	// 否则将自己注册为默认广播日志器后端
	else {
		ILoggerManager* lm = (ILoggerManager*)pch::api::FindObject(PCH_DEFAULT_LOGGERMANAGER);
		if (lm) {
			lm->addDefaultLogger(this, nullptr);
		}
	}

	return nullptr;  // 无需回复
}

PCH_END_NAMESPACE
