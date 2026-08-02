/**
 * @file loggerManager.h
 * @brief 日志子系统头文件：声明控制台写入器、级别过滤日志器及日志管理器类
 * @details
 * 日志管理器是 PCH 内置日志能力的组织中心。
 * 它通过名称维护 LoggerLevelCtrl，支持以 '.' 分隔的名称层级与父配置继承，
 * 并通过 mergeLogger 实现子树配置同步。同时实现 IMessageHandler 以处理日志相关的系统消息。
 * WriteLog 及各实现的格式化和写入管道详情见 loggerManager.cpp。
 */
#pragma once
#include "interface.h"
#include <atomic>
#include <cstdarg>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <vector>

PCH_BEGIN_NAMESPACE

// 日志级别"未设置"哨兵值（LoggerLevelCtrl::_logLevel 默认值）
constexpr LogLevel LOG_LEVEL_UNSET = static_cast<LogLevel>(-1);

// 日志入口函数，在 loggerManager.cpp 中定义，将日志写入标准输出
void    WriteLog(LogLevel level, const char* fmt, ...);
// 单次日志格式化缓冲区的最大长度（含尾部预留空间）
#define LOG_BUFFER_LENGTH		4096

// 向标准输出写入日志文本（ILoggerWrite 实现），是 LoggerManager 的默认后端之一
class ConsoleLoggerWrite : public ILoggerWrite
{
public:
	// 向标准输出写入日志文本
	virtual void writeLog(LogLevel level, const char* logText)
	{
		(void)level;
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
 * @brief 级别过滤+多输出的 ILogger 实现，用于命名日志器或默认日志器
 * @details
 * 低于 _logLevel 的消息被丢弃；_loggerWrite 包含（写入器，可选格式化器）对。
 * 部分成员由 LoggerManager 组装路径读/写，不完全是纯封装设计。
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
		writeLog(LogLevel::Fatal, fmt, vaList);
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
		writeLog(LogLevel::Error, fmt, vaList);
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
		writeLog(LogLevel::Warning, fmt, vaList);
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
		writeLog(LogLevel::Information, fmt, vaList);
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
		writeLog(LogLevel::Debug, fmt, vaList);
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
		writeLog(LogLevel::Trace, fmt, vaList);
		// 结束解析变量列表
		va_end(vaList);
	}

	// 写入日志
	virtual void writeLog(LogLevel logLevel, const char* fmt, va_list vaList)
	{
		// 如果日志级别高于当前级别，则丢弃
		if (_logLevel > logLevel)
		{
			return;
		}
		// 如果系统未关闭且有输出后端，则写入日志
		if (!_systemIsShutdown && _loggerWrite.size() > 0)
		{
			char logBuffer[LOG_BUFFER_LENGTH];
			// 在日志缓冲区末尾设置空终止符
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
				// 如果格式化器可用，则格式化日志
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
	LogLevel _logLevel = LOG_LEVEL_UNSET;  // 日志级别（未设置时继承父级/默认）
	std::string _loggerName;                  // 日志器名称
	bool        _isGet = false;               // 是否已被获取
};

/**
 * @class LoggerManager
 * @brief PCH 日志枢纽：实现 ILoggerManager，并可通过消息调整日志行为（IMessageHandler）
 * @details
 * 默认日志器名称为 "PCHCore"，构造时挂载 _consoleLogger。
 * _loggerMap 和默认条目的更新受 _mutex 保护。
 * 接口语义与 interface.h 中的 ILoggerManager 一致；handleMessage 详见 loggerManager.cpp。
 */
class LoggerManager: public ILoggerManager, public IMessageHandler
{
public:
	/**
	 * @brief 构造默认日志枢纽
	 * @details 默认日志器名称为 "PCHCore"，挂载 _consoleLogger，级别为 LogLevel::Trace
	 */
	LoggerManager();

	/** @brief 析构函数 */
	virtual ~LoggerManager();

	/**
	 * @brief 按名称解析并返回 ILogger*（实际是 LoggerLevelCtrl）
	 * @param[in] logName 日志器名称；null 或空字符串返回默认日志器
	 * @return 合并父配置后的日志器；设置 _isGet，之后部分配置接口拒绝变更
	 */
	virtual ILogger* getLogger(const char* logName);

	/**
	 * @brief 按 '.' 分段向上搜索 _loggerMap，合并写入列表和级别，必要时创建叶子项
	 * @param[in] logName 完整日志器名称（可能包含 '.' 层级）
	 * @return 选中的 LoggerLevelCtrl*；无匹配时使用默认日志器的写入和级别
	 */
	ILogger* mergeLogger(const char* logName);

	/**
	 * @brief 设置默认日志器（"PCHCore"）的最低输出级别
	 * @param[in] level 日志级别
	 * @return 始终返回 true
	 */
	virtual bool setDefaultLoggerLevel(LogLevel level);

	/**
	 * @brief 用单个（write, format）对替换默认日志器的输出后端
	 * @param[in] loggerWrite 后端写入器，不可为 nullptr
	 * @param[in] logFormat 可选格式化器
	 * @return 成功返回 true；如果 loggerWrite 为空或默认日志器已通过 getDefaultLogger 获取则返回 false
	 */
	virtual bool setDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat);

	/**
	 * @brief 为默认日志器追加一对输出后端（去重）
	 * @param[in] loggerWrite 后端写入器，不可为 nullptr
	 * @param[in] logFormat 可选格式化器
	 * @return 成功返回 true；如果相同 loggerWrite 已存在或默认日志器已被获取则返回 false
	 */
	virtual bool addDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat);

	/**
	 * @brief 清空默认日志器的输出后端列表
	 * @return 成功返回 true；如果默认日志器已通过 getDefaultLogger 获取则返回 false
	 */
	virtual bool clearDefaultLogger();

	/**
	 * @brief 设置（替换）命名日志器的输出：不存在则插入，已存在则清空后设置一对
	 * @param[in] logName 日志器名称，非空
	 * @param[in] loggerWrite 后端写入器，不可为 nullptr
	 * @param[in] logFormat 可选格式化器
	 * @return 成功返回 true；如果名称为空、loggerWrite 为空或名称已通过 getLogger 使用则返回 false
	 */
	virtual bool setLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat);

	/**
	 * @brief 为现有命名日志器追加一对输出后端（mergeLogger 确保条目存在）
	 * @param[in] logName 日志器名称，非空
	 * @param[in] loggerWrite 后端写入器，不可为 nullptr
	 * @param[in] logFormat 可选格式化器
	 * @return 成功返回 true；如果条目不存在、后端重复或名称已被获取则返回 false
	 */
	virtual bool addLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat);

	/**
	 * @brief 清空命名日志器的输出后端并从 _loggerMap 中移除
	 * @param[in] logName 日志器名称，非空
	 * @return 成功返回 true；如果名称为空或名称已被获取则返回 false
	 */
	virtual bool clearLogger(const char* logName);

	/**
	 * @brief 设置（或创建）命名日志器的最低输出级别
	 * @param[in] logName 日志器名称，非空
	 * @param[in] level 日志级别
	 * @return 名称为空时返回 false；否则写入级别并返回 true
	 */
	virtual bool setLoggerLevel(const char* logName,LogLevel level);

	/**
	 * @brief 处理系统消息：收到 SystemReady 时将全局 WriteLog 的 _PCHLogger 指向 "PCHCore"；
	 *        收到 SystemShutdown 时清空并标记关闭
	 * @param[in] msg 消息指针
	 * @return 始终返回 nullptr
	 */
	const pch::IMessage* handleMessage(const pch::IMessage* msg);

	/**
	 * @brief 返回指向内置默认 LoggerLevelCtrl（名称为 "PCHCore"）的指针
	 * @return &_defaultLogger；设置 _isGet，之后 setDefaultLogger/addDefaultLogger/clearDefaultLogger 将失败
	 */
	ILogger* getDefaultLogger()
	{
		_defaultLogger._isGet = true;
		return &_defaultLogger;
	}
private:
	std::mutex           _mutex;                        // 保护 _loggerMap 和默认日志器配置变更的互斥锁
	ConsoleLoggerWrite   _consoleLogger;                // 控制台写入器，实现 ILoggerWrite 接口
	LoggerLevelCtrl      _defaultLogger;                // 默认日志器，实现 ILogger 接口
	std::map<std::string, LoggerLevelCtrl> _loggerMap;  // 命名日志器表，名称到 LoggerLevelCtrl 的映射
};

PCH_END_NAMESPACE
