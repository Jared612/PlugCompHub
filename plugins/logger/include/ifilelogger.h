/**
 * @file ifilelogger.h
 * @brief 文件日志接口：扩展 ILoggerWrite，增加路径和级别管理
 */
#pragma once
#include "interface.h"
#include <string>

PCH_BEGIN_NAMESPACE

class IFileLogger : public ILoggerWrite
{
public:
	/**
	 * @brief 在指定路径打开日志文件，按最低级别过滤
	 * @param filePath 绝对或相对文件路径
	 * @param minLevel 仅记录此级别及以上的日志
	 * @return 成功返回 true
	 */
	virtual bool open(const char* filePath, LogLevel minLevel = LogLevel::Trace) = 0;

	/**
	 * @brief 设置最低日志级别；低于此级别的日志将被静默丢弃
	 */
	virtual void setMinLevel(LogLevel level) = 0;

	/**
	 * @brief 关闭日志文件并刷新
	 */
	virtual void close() = 0;

	/**
	 * @brief 检查文件当前是否已打开可写
	 */
	virtual bool isOpen() const = 0;
};

PCH_END_NAMESPACE
