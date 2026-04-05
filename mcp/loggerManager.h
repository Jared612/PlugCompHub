/**
 * @file loggerManager.h
 * @brief 日志子系统头文件，声明控制台写出、按级别过滤的日志器及日志管理器类。
 * @details
 * 日志管理器为 MCP 内置日志能力的组织中心，按名维护 `LoggerLevelCtrl`，支持以 `.` 分段的名字继承父级配置，
 * 并与 `mergeLogger` 等接口合并子树配置；同时实现 `IMessageHandler` 以处理与日志相关的系统消息。
 * `WriteLog` 与各类的实现、格式化与写出管道细节见同目录 `loggerManager.cpp`。
 */
#pragma once
#include "interface.h"
#include "internal.h"
#include <cstdarg>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

MCP_BEGIN_NAMESPACE


void    WriteLog(McpLogLevel level, const char* fmt, ...);  	// 日志入口函数，定义在 `loggerManager.cpp`，用于将日志写入到标准输出
#define LOG_BUFFER_LENGTH		4096                            // 单条日志格式化缓冲区的最大长度（含结尾预留）

// 将日志文本写到标准输出的 `ILoggerWrite` 实现（`LoggerManager` 默认后端之一），实现 `ILoggerWrite` 接口
class ConsoleLoggerWrite : public ILoggerWrite
{
public:
	// 将日志文本写到标准输出
	virtual void writeLog(McpLogLevel level, const char* logText)
	{
		printf("%s", logText);
		printf("\n");
	}

	// 刷新缓冲区
	virtual void flush()
	{
	}
};

/**
 * @class LoggerLevelCtrl
 * @brief 带级别与多路输出的 `ILogger` 实现，供具名或默认日志器使用。
 * @details
 * `_logLevel` 高于当前消息级别则丢弃；`_loggerWrite` 中每对为「写入器 + 可选格式器」。
 * 部分成员为 `LoggerManager` 装配路径所读写，非纯粹封装。
 */
class LoggerLevelCtrl : public ILogger
{
public:
	// 构造函数，初始化日志器名称
	LoggerLevelCtrl(const char* logName)
	{
		_loggerName = logName;
	}
	
	// 写入致命错误日志
	void fatal(const char* fmt, ...)
	{
		// 初始化变量列表
		va_list vaList;
		// 开始解析变量列表
		va_start(vaList, fmt);	
		// 写入致命错误日志
		writeLog(McpLogLevel::Fatal, fmt, vaList);
		// 结束解析变量列表
		va_end(vaList);
	}

	// 写入错误日志
	void error(const char* fmt, ...)
	{
		// 初始化变量列表
		va_list vaList;
		// 开始解析变量列表
		va_start(vaList, fmt);
		// 写入错误日志
		writeLog(McpLogLevel::Error, fmt, vaList);
		// 结束解析变量列表
		va_end(vaList);
	}

	// 写入警告日志
	void warn(const char* fmt, ...)
	{
		// 初始化变量列表
		va_list vaList;
		// 开始解析变量列表
		va_start(vaList, fmt);
		// 写入警告日志
		writeLog(McpLogLevel::Warning, fmt, vaList);
		// 结束解析变量列表
		va_end(vaList);
	}

	// 写入信息日志
	void info(const char* fmt, ...)
	{
		// 初始化变量列表
		va_list vaList;
		// 开始解析变量列表
		va_start(vaList, fmt);
		// 写入信息日志
		writeLog(McpLogLevel::Information, fmt, vaList);
		// 结束解析变量列表
		va_end(vaList);
	}

	// 写入调试日志
	void debug(const char* fmt, ...)
	{	
		// 初始化变量列表
		va_list vaList;
		// 开始解析变量列表
		va_start(vaList, fmt);
		// 写入调试日志
		writeLog(McpLogLevel::Debug, fmt, vaList);
		// 结束解析变量列表
		va_end(vaList);
	}

	// 写入跟踪日志
	void trace(const char* fmt, ...)
	{
		// 初始化变量列表
		va_list vaList;
		// 开始解析变量列表
		va_start(vaList, fmt);
		// 写入跟踪日志
		writeLog(McpLogLevel::Trace, fmt, vaList);
		// 结束解析变量列表
		va_end(vaList);
	}

	// 写入日志
	virtual void writeLog(McpLogLevel logLevel, const char* fmt, va_list vaList)
	{
		// 如果日志级别大于当前日志级别，则不写入
		if (_logLevel > logLevel)
		{
			return;
		}
		// 如果系统未关闭且有输出后端，则写入日志
		if (!_systemIsShutdown && _loggerWrite.size() > 0)
		{
			char logBuffer[LOG_BUFFER_LENGTH];
			// 设置日志缓冲区末尾为空字符
			logBuffer[LOG_BUFFER_LENGTH - 1] = 0;
			// 遍历输出后端
			for (int i = 0 ; i < _loggerWrite.size() ; i++)
			{
				// 获取输出后端
				ILoggerWrite* loggerWrite = _loggerWrite[i].first;
				// 获取格式化器
				ILoggerFormat* loggerForamt = _loggerWrite[i].second;
				// 初始化变量列表
				va_list vaListTmp;
				// 复制变量列表
				va_copy(vaListTmp, vaList);
				// 初始化写入长度
				int writeLen = 0;
				// 如果格式化器不为空，则格式化日志
				if (loggerForamt)
				{
					// 格式化日志
					writeLen = loggerForamt->formatLog(logBuffer, sizeof(logBuffer) - 1, _loggerName.c_str(), logLevel, fmt, vaListTmp);
				}
				else
				{
					// 格式化日志
					writeLen = vsnprintf(logBuffer, sizeof(logBuffer) - 1, fmt, vaListTmp);
				}
				// 写入日志
				loggerWrite->writeLog(logLevel, logBuffer);
			}
		}
		// 如果系统已关闭或没有输出后端，则写入标准输出
		else
		{
			// 写入标准输出
			vprintf(fmt, vaList);
			printf("\n");
		}
	}

	bool _systemIsShutdown = false;										// 系统是否已关闭
	std::vector<std::pair<ILoggerWrite*,ILoggerFormat*> > _loggerWrite;	// 输出后端列表
	McpLogLevel _logLevel = (McpLogLevel)-1;  // 日志级别
	std::string _loggerName;                  // 日志器名称
	bool        _isGet = false;               // 是否已获取
};

/**
 * @class LoggerManager
 * @brief MCP 日志中枢：实现 `ILoggerManager`，并可通过消息调整日志行为（`IMessageHandler`）。
 * @details
 * 默认日志器名为 `mcpCore`，构造时挂接 `_consoleLogger`。对 `_loggerMap` 与默认项的更新由 `_mutex` 保护。
 * 接口含义与 `internal.h` 中 `ILoggerManager` 一致；`handleMessage` 详见 `loggerManager.cpp`。
 */
class LoggerManager: public ILoggerManager, public IMessageHandler
{
public:
	/**
	 * @brief 构造默认日志中枢。
	 * @details 默认日志器名为 `mcpCore`，挂接 `_consoleLogger`，级别为 `McpLogLevel::Trace`。
	 */
	LoggerManager();

	/** @brief 析构。 */
	virtual ~LoggerManager();

	/**
	 * @brief 按名解析并返回 `ILogger*`（实际为 `LoggerLevelCtrl`）。
	 * @param[in] logName 日志器名；空指针或空串时返回默认日志器
	 * @return 合并父级配置后的日志器；会置 `_isGet`，此后部分配置接口将拒绝修改
	 */
	virtual ILogger* getLogger(const char* logName);

	/**
	 * @brief 按 `.` 分段向上查找 `_loggerMap`，合并写出列表与级别，必要时创建叶子项。
	 * @param[in] logName 完整 logger 名（可含 `.` 层级）
	 * @return 选中的 `LoggerLevelCtrl*`；未匹配时使用默认日志器的写出与级别
	 */
	ILogger* mergeLogger(const char* logName);

	/**
	 * @brief 设置默认日志器（`mcpCore`）的最低输出级别。
	 * @param[in] level 级别
	 * @return 恒为 true
	 */
	virtual bool setDefaultLoggerLevel(McpLogLevel level);

	/**
	 * @brief 将默认日志器的输出后端替换为单一 `(write, format)` 对。
	 * @param[in] loggerWrite 写出器，不可为 nullptr
	 * @param[in] logFormat 可选格式化器
	 * @return 成功 true；`loggerWrite` 为空或默认器已被 `getDefaultLogger` 获取过则 false
	 */
	virtual bool setDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat);

	/**
	 * @brief 向默认日志器追加一对输出后端（去重）。
	 * @param[in] loggerWrite 写出器，不可为 nullptr
	 * @param[in] logFormat 可选格式化器
	 * @return 成功 true；已存在相同 `loggerWrite` 或默认器已被获取则 false
	 */
	virtual bool addDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat);

	/**
	 * @brief 清空默认日志器的输出后端列表。
	 * @return 成功 true；若默认器已被 `getDefaultLogger` 获取过则 false
	 */
	virtual bool clearDefaultLogger();

	/**
	 * @brief 为具名 logger 设置（替换）输出：无则插入，有则清空后设为一对后端。
	 * @param[in] logName 日志器名，非空
	 * @param[in] loggerWrite 写出器，不可为 nullptr
	 * @param[in] logFormat 可选格式化器
	 * @return 成功 true；名为空、`loggerWrite` 为空或该名已被 `getLogger` 使用过则 false
	 */
	virtual bool setLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat);

	/**
	 * @brief 向已存在的具名 logger 追加一对输出后端（先 `mergeLogger` 确保表项存在）。
	 * @param[in] logName 日志器名，非空
	 * @param[in] loggerWrite 写出器，不可为 nullptr
	 * @param[in] logFormat 可选格式化器
	 * @return 成功 true；表项不存在、重复后端或该名已被获取则 false
	 */
	virtual bool addLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat);

	/**
	 * @brief 清除指定名的输出后端并从 `_loggerMap` 移除该项。
	 * @param[in] logName 日志器名，非空
	 * @return 成功 true；名为空或该名已被获取则 false
	 */
	virtual bool clearLogger(const char* logName);

	/**
	 * @brief 设置（或创建）具名 logger 的最低输出级别。
	 * @param[in] logName 日志器名，非空
	 * @param[in] level 级别
	 * @return 名为空返回 false；否则写入级别并返回 true
	 */
	virtual bool setLoggerLevel(const char* logName,McpLogLevel level);

	/**
	 * @brief 处理系统消息：`SystemReady` 时将全局 `WriteLog` 用的 `_mcpLogger` 指到 `mcpCore`；`SystemShutdown` 时清空并标记关闭。
	 * @param[in] msg 消息指针
	 * @return 恒为 nullptr
	 */
	const mcp::IMessage* handleMessage(const mcp::IMessage* msg);

	/**
	 * @brief 返回内置默认 `LoggerLevelCtrl`（名 `mcpCore`）的指针。
	 * @return `&_defaultLogger`；会置 `_isGet`，之后 `setDefaultLogger`/`addDefaultLogger`/`clearDefaultLogger` 将失败
	 */
	ILogger* getDefaultLogger()
	{
		_defaultLogger._isGet = true;
		return &_defaultLogger;
	}
private:
	std::mutex           _mutex;                        // 互斥锁，保护 `_loggerMap` 及对默认日志器配置的修改
	ConsoleLoggerWrite   _consoleLogger;                // 控制台写出器，实现 `ILoggerWrite` 接口
	LoggerLevelCtrl      _defaultLogger;                // 默认日志器，实现 `ILogger` 接口
	std::map<std::string, LoggerLevelCtrl> _loggerMap;  // 具名日志器表，完整 logger 名 → `LoggerLevelCtrl`
};

MCP_END_NAMESPACE