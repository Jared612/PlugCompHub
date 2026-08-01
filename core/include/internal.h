/**
 * @file internal.h
 * @brief 内核侧管理抽象：插件、组件、对象、消息、日志、环境接口
 * @details 随源码编译；比外置安装头文件更偏实现；由内置模块实现包含
 */

#pragma once

#include "interface.h"

PCH_BEGIN_NAMESPACE

/**
 * @brief 插件信息抽象接口（内核/应用框架使用；不随外部简化 SDK 安装）
 */
class IPlugin
{
public:
	virtual const char* getName() = 0;
	virtual const char* getVersion() = 0;
	virtual const char* getDescription() = 0;
	virtual const char* getPath() = 0;
};

/**
 * @brief 插件管理器接口（内核/应用框架使用）
 */
class IPluginManager
{
public:
	virtual IPlugin* loadPlugin(const char* path) = 0;
	virtual ErrorCode unloadPlugin(IPlugin* plugin) = 0;
};

/**
 * @brief 组件管理器接口（内核使用）
 */
class IComponentManager
{
public:
	virtual ErrorCode registerComponent(ComponentInfo* compInfo)     = 0;  // 注册单个组件
	virtual ErrorCode registerComponents(ComponentInfo** cmptable)   = 0;  // 批量注册组件
	virtual ErrorCode unregisterComponent(ComponentInfo* compInfo)   = 0;  // 注销单个组件（按信息）
	virtual ErrorCode unregisterComponents(ComponentInfo** cmptable) = 0;  // 批量注销组件
	virtual ErrorCode unregisterComponent(const char* componentID)   = 0;  // 注销单个组件（按 ID）
	virtual ComponentInfo* getComponentInf(const char* componentID)  = 0;  // 按 ID 查询组件信息
};

/** IObjectArray / IObjectManager 在 interface.h 中（供 core.h 内联使用） */

/**
 * @brief 日志管理器接口（内核使用）
 */
class ILoggerManager
{
public:
	virtual ILogger* getLogger(const char* logName)                                                            = 0;  // 按指定名称获取日志器
	virtual ILogger* getDefaultLogger()                                                                        = 0;  // 获取默认日志器
	virtual bool setDefaultLoggerLevel(LogLevel level)                                                      = 0;  // 设置默认日志器级别
	virtual bool setDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr)               = 0;  // 设置默认日志器输出
	virtual bool addDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr)               = 0;  // 添加默认日志器输出
	virtual bool setLoggerLevel(const char* logName, LogLevel level)                                        = 0;  // 设置指定日志器级别
	virtual bool setLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr) = 0;  // 设置指定日志器输出
	virtual bool addLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr) = 0;  // 添加指定日志器输出
};

/**
 * @brief 环境变量接口（内核使用）
 */
class IEnvironment
{
public:
	virtual const char* get(const char* key, ErrorCode* errCode = nullptr) = 0;  // 读取环境变量
	virtual ErrorCode set(const char* key, const char* value)              = 0;  // 设置环境变量
};

PCH_END_NAMESPACE
