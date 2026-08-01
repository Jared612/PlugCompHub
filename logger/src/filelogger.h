/**
 * @file filelogger.h
 * @brief 基于 spdlog 的文件日志实现
 */
#pragma once
#include "ifilelogger.h"
#include <mutex>

PCH_BEGIN_NAMESPACE

class FileLogger : public IFileLogger, public IMessageHandler
{
public:
	FileLogger();
	~FileLogger();

	bool   open(const char* filePath, LogLevel minLevel = LogLevel::Trace) override;
	void   setMinLevel(LogLevel level) override;
	void   close() override;
	bool   isOpen() const override;

	void   writeLog(pch::LogLevel level, const char* logText) override;
	void   flush() override;

	const IMessage* handleMessage(const IMessage* msg) override;

private:
	void*  _loggerImpl = nullptr;  // 不透明的 spdlog::logger* 指针
	LogLevel _minLevel  = LogLevel::Trace;
	bool     _isOpen    = false;
	std::string _logName;          // 关联的命名日志器名称，空则为默认广播日志器
	mutable std::mutex _mutex;
};

PCH_END_NAMESPACE
