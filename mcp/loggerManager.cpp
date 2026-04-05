#include <string.h>
#include "loggerManager.h"
#include <vector>

MCP_REGISTER_COMPONENT(mcp::LoggerManager, MCP_LOGGERMANAGER_ID)

MCP_BEGIN_NAMESPACE


LoggerLevelCtrl* _mcpLogger = nullptr;	// 全局日志器

/**
* @brief 写入日志
* @param[in] level 日志级别
* @param[in] fmt 日志格式
* @return 无
*/
void WriteLog(McpLogLevel level, const char* fmt, ...)
{
	// 初始化变量列表
	va_list vaList;
	// 开始解析变量列表	
	va_start(vaList, fmt);
	// 如果日志管理器不为空，则写入日志
	if (_mcpLogger)
		// 写入日志
		_mcpLogger->writeLog(level, fmt, vaList);
	else{
		vprintf(fmt, vaList);
		printf("\n");
	}
	// 结束解析变量列表
	va_end(vaList);
}

/*	*
* @brief 构造日志管理器
* @return 日志管理器
*/
LoggerManager::LoggerManager() : _defaultLogger("mcpCore")
{
	// 添加默认日志器输出后端
	_defaultLogger._loggerWrite.push_back(std::make_pair(&_consoleLogger, nullptr));
	// 设置默认日志器级别
	_defaultLogger._logLevel = McpLogLevel::Trace;
}

LoggerManager::~LoggerManager()
{

}

/**
* @brief 分割日志器名称
* @param[in] logName 日志器名称
* @param[out] names 日志器名称列表
*/
void splicLogName(const char* logName,std::vector<std::string> &names)
{
	std::string nameStr = logName;
	static char delim = '.';
	// 添加日志器名称	
	names.push_back(nameStr);
	// 遍历日志器名称
	for (int i = nameStr.length() - 1; i >= 0; i--)
	{
		if (nameStr.c_str()[i] == delim)
		{
			// 截取日志器名称
			nameStr = nameStr.substr(0, i);
			// 添加日志器名称
			names.push_back(nameStr);
		}
	}
}

/**
* @brief 获取日志器
* @param[in] logName 日志器名称
* @return 日志器
*/
ILogger* LoggerManager::getLogger(const char* logName)
{
	// 如果日志器名称为空，则返回默认日志器
	if (!logName || strlen(logName) <= 0)
		return &_defaultLogger;
	// 合并日志器
	LoggerLevelCtrl* retLogger = (LoggerLevelCtrl*)mergeLogger(logName);
	// 设置日志器已获取
	retLogger->_isGet = true;
	return retLogger;
}

/**
* @brief 合并日志器
* @param[in] logName 日志器名称
* @return 合并后的日志器
*/
ILogger* LoggerManager::mergeLogger(const char* logName)
{
	// 分割日志器名称
	std::vector<std::string> logNameVec;
	splicLogName(logName, logNameVec);

	// 选择日志器
	LoggerLevelCtrl* selectLogger = nullptr;

	// 加锁，防止多线程同时访问
	_mutex.lock();
	// 遍历日志器名称
	for (int i = 0; i < logNameVec.size(); i++) {
		auto it = _loggerMap.find(logNameVec[i]);
		if (it != _loggerMap.end()) {
			// 如果选择日志器为空，则选择当前日志器
			if (selectLogger == nullptr) {
				selectLogger = &it->second;
			}
			else {
				// 如果选择日志器输出后端为空，则选择当前日志器输出后端
				if (selectLogger->_loggerWrite.size() == 0)
					selectLogger->_loggerWrite = it->second._loggerWrite;
				// 如果选择日志器级别为空，则选择当前日志器级别
				if (selectLogger->_logLevel == (McpLogLevel)-1)
					selectLogger->_logLevel = it->second._logLevel;
			}
			// 如果选择日志器输出后端不为空且选择日志器级别为空，则跳出循环
			if (selectLogger->_loggerWrite.size() > 0 && selectLogger->_logLevel == (McpLogLevel)-1)
				break;
		}	
		// 如果当前是第一个日志器名称，则创建日志器
		else if (i == 0) {
			// 创建日志器
			LoggerLevelCtrl log(logName);
			_loggerMap.insert(std::make_pair(logName, log));
			it = _loggerMap.find(logNameVec[i]);
			selectLogger = &it->second;
		}
	}
	// 如果选择日志器为空，则选择默认日志器
	if (selectLogger == nullptr)
		selectLogger = &_defaultLogger;
	// 如果选择日志器输出后端为空，则选择默认日志器输出后端
	if (selectLogger->_loggerWrite.size() == 0)
		selectLogger->_loggerWrite = _defaultLogger._loggerWrite;
	// 如果选择日志器级别为空，则选择默认日志器级别
	if (selectLogger->_logLevel == (McpLogLevel)-1)
		selectLogger->_logLevel = _defaultLogger._logLevel;
	// 解锁，防止多线程同时访问
	_mutex.unlock();
	// 返回选择日志器
	return selectLogger;
}

/**
* @brief 设置默认日志器级别
* @param[in] level 日志级别
* @return 成功返回 true，失败返回 false
*/
bool LoggerManager::setDefaultLoggerLevel(McpLogLevel level)
{
	// 加锁，防止多线程同时访问
	_mutex.lock();
	// 设置默认日志器级别
	_defaultLogger._logLevel = level;
	// 解锁，防止多线程同时访问
	_mutex.unlock();
	// 返回成功
	return true;
}

bool LoggerManager::setDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat)
{
	// 如果日志器输出后端为空，则返回 false
	if (!loggerWrite)	{
		return false;
	}
	// 加锁，防止多线程同时访问
	_mutex.lock();
	// 如果默认日志器已获取，则返回 false
	if (_defaultLogger._isGet){
		_mutex.unlock();
		return false;
	}
	// 清除默认日志器输出后端
	_defaultLogger._loggerWrite.clear();
	// 添加默认日志器输出后端
	_defaultLogger._loggerWrite.push_back(std::make_pair(loggerWrite,logFormat));
	// 解锁，防止多线程同时访问
	_mutex.unlock();
	return true;
}

/**
* @brief 添加默认日志器
* @param[in] loggerWrite 日志器输出后端
* @param[in] logFormat 日志器格式化器
* @return 成功返回 true，失败返回 false
*/
bool LoggerManager::addDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat)
{
	// 如果日志器输出后端为空，则返回 false
	if (!loggerWrite) {
		return false;
	}

	// 加锁，防止多线程同时访问	
	_mutex.lock();
	// 如果默认日志器已获取，则返回 false
	if (_defaultLogger._isGet){
		_mutex.unlock();
		return false;
	}
	// 遍历默认日志器输出后端
	for (int i = 0 ; i < _defaultLogger._loggerWrite.size() ; i++) {
		// 如果默认日志器输出后端已存在，则返回 false
		if (_defaultLogger._loggerWrite[i].first == loggerWrite) {
			_mutex.unlock();
			return false;
		}
	}
	// 添加默认日志器输出后端
	_defaultLogger._loggerWrite.push_back(std::make_pair(loggerWrite, logFormat));
	// 解锁，防止多线程同时访问
	_mutex.unlock();
	return true;
}

/**
* @brief 清除默认日志器
* @return 成功返回 true，失败返回 false
*/
bool LoggerManager::clearDefaultLogger()
{
	// 加锁，防止多线程同时访问
	_mutex.lock();
	// 如果默认日志器已获取，则返回 false
	if (_defaultLogger._isGet){
		_mutex.unlock();
		return false;
	}
	// 清除默认日志器输出后端
	_defaultLogger._loggerWrite.clear();
	// 解锁，防止多线程同时访问
	_mutex.unlock();
	return true;
}

/**
* @brief 设置日志器
* @param[in] logName 日志器名称
* @param[in] loggerWrite 日志器输出后端
* @param[in] logFormat 日志器格式化器
* @return 成功返回 true，失败返回 false
*/
bool LoggerManager::setLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat)
{
	// 如果日志器输出后端为空或日志器名称为空，则返回 false
	if (!loggerWrite || !logName || strlen(logName) <= 0)
		return false;

	// 如果日志器输出后端为空，则设置为默认输出后端
	if (loggerWrite == nullptr)
		loggerWrite = &_consoleLogger;

	// 加锁，防止多线程同时访问
	_mutex.lock();
	// 查找日志器

	auto it = _loggerMap.find(logName);
	// 如果日志器不存在，则创建日志器
	if (it == _loggerMap.end())	{
		LoggerLevelCtrl log(logName);
		log._loggerWrite.push_back(std::make_pair(loggerWrite, logFormat));
		_loggerMap.insert(std::make_pair(logName, log));
		_mutex.unlock();
		return true;
	}
	// 如果日志器已获取，则返回 false
	else {
		// 如果日志器已获取，则返回 false
		if (it->second._isGet) {
			_mutex.unlock();
			return false;
		}
		it->second._loggerWrite.clear();
		it->second._loggerWrite.push_back(std::make_pair(loggerWrite, logFormat));
	}
	// 清除日志器输出后端
	_mutex.unlock();
	return true;
}

/**
* @brief 添加日志器
* @param[in] logName 日志器名称
* @param[in] loggerWrite 日志器输出后端
* @param[in] logFormat 日志器格式化器
* @return 成功返回 true，失败返回 false
*/
bool LoggerManager::addLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat)
{
	// 如果日志器输出后端为空或日志器名称为空，则返回 false
	if (!loggerWrite || !logName || strlen(logName) <= 0)
		return false;

	// 如果日志器输出后端为空，则设置为默认输出后端
	if (loggerWrite == nullptr)
		loggerWrite = &_consoleLogger;

	// 合并日志器
	mergeLogger(logName);

	// 加锁，防止多线程同时访问
	_mutex.lock();
	// 查找日志器
	auto it = _loggerMap.find(logName);
	// 如果日志器不存在，则返回 false	
	if (it == _loggerMap.end()) {
		// 解锁，防止多线程同时访问
		_mutex.unlock();
		return false;
	}
	// 如果日志器已获取，则返回 false
	if (it->second._isGet) {
		_mutex.unlock();
		return false;
	}
	// 遍历日志器输出后端
	for (int i = 0; i < it->second._loggerWrite.size(); i++) {
		// 如果日志器输出后端已存在，则返回 false
		if (it->second._loggerWrite[i].first == loggerWrite) {
			_mutex.unlock();
			return false;
		}
	}
	// 添加日志器输出后端
	it->second._loggerWrite.push_back(std::make_pair(loggerWrite, logFormat));
	// 解锁，防止多线程同时访问
	_mutex.unlock();
	return true;
}

/**
* @brief 清除日志器
* @param[in] logName 日志器名称
* @return 成功返回 true，失败返回 false
*/
bool LoggerManager::clearLogger(const char* logName)
{
	// 如果日志器名称为空，则返回 false
	if (!logName || strlen(logName) <= 0)
		return false;

	// 加锁，防止多线程同时访问	
	_mutex.lock();
	// 查找日志器
	auto it = _loggerMap.find(logName);
	// 如果日志器存在，则清除日志器
	if (it != _loggerMap.end()) {
		// 如果日志器已获取，则返回 false
		if (it->second._isGet) {
			_mutex.unlock();
			return false;
		}
		// 清除日志器输出后端
		it->second._loggerWrite.clear();
		// 删除日志器	
		_loggerMap.erase(it);
	}
	// 解锁，防止多线程同时访问
	_mutex.unlock();
	return true;
}

/**
* @brief 设置日志器级别
* @param[in] logName 日志器名称
* @param[in] level 日志级别
* @return 成功返回 true，失败返回 false
*/
bool LoggerManager::setLoggerLevel(const char* logName, McpLogLevel level)
{
	// 如果日志器名称为空，则返回 false
	if (!logName || strlen(logName) <= 0)
		return false;

	// 加锁，防止多线程同时访问
	_mutex.lock();

	// 查找日志器
	auto it = _loggerMap.find(logName);
	// 如果日志器不存在，则创建日志器
	if (it == _loggerMap.end()) {
		// 创建日志器
		LoggerLevelCtrl log(logName);
		// 设置日志器级别
		log._logLevel = level;
		// 插入日志器
		_loggerMap.insert(std::make_pair(logName, log));
	}
	else {
		// 设置日志器级别
		it->second._logLevel = level;
	}
	// 解锁，防止多线程同时访问
	_mutex.unlock();
	return true;
}

/**
* @brief mcp消息处理函数
* @param[in] msg 消息指针
* @return 恒为 nullptr
*/
const mcp::IMessage* LoggerManager::handleMessage(const mcp::IMessage* msg)
{
	// 如果消息为空，则返回 nullptr
	if (msg == nullptr) {
		return nullptr;
	}

	// 根据消息类型进行处理
	switch (msg->getCode()) 
	{
		// 系统就绪消息
		case mcp::SystemReady:
		{
			_mcpLogger = (LoggerLevelCtrl*)this->getLogger("mcpCore");
		}
		break;
		// 系统关闭消息
		case mcp::SystemShutdown:
		{
			// 清空全局日志器
			_mcpLogger = nullptr;
			// 设置默认日志器系统已关闭
			_defaultLogger._systemIsShutdown = true;
			// 遍历日志器列表
			for (auto it = _loggerMap.begin() ; it != _loggerMap.end() ; it++)
			{
				// 设置日志器系统已关闭
				it->second._systemIsShutdown = true;
			}
		}
		break;
	}
	return nullptr;
}

MCP_END_NAMESPACE