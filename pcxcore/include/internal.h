/**
 * @file internal.h
 * @brief 内核使用的管理面抽象：插件、组件、对象、消息、日志、环境等接口。
 * @details 随源码参与编译；与对外安装头相比更偏实现侧，内置模块实现时会包含。
 */

#pragma once

#include "interface.h"

PCX_BEGIN_NAMESPACE

/**
 * @brief 插件信息抽象接口（内核 / 应用框架使用，不随对外精简 SDK 单独安装）。
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
 * @brief 插件管理接口（内核 / 应用框架使用）。
 */
class IPluginManager
{
public:
	virtual IPlugin* loadPlugin(const char* path) = 0;
	virtual ErrorCode unloadPlugin(IPlugin* plugin) = 0;
};

/**
 * @brief 组件管理接口（内核使用）
 */
class IComponentManager
{
public:
	virtual ErrorCode registerComponent(ComponentInfo* compInfo)     = 0;  // 注册单个组件
	virtual ErrorCode registerComponents(ComponentInfo** cmptable)   = 0;  // 批量注册组件
	virtual ErrorCode unregisterComponent(ComponentInfo* compInfo)   = 0;  // 注销单个组件（按信息）
	virtual ErrorCode unregisterComponents(ComponentInfo** cmptable) = 0;  // 批量注销组件
	virtual ErrorCode unregisterComponent(const char* componentID)   = 0;  // 注销单个组件（按ID）
	virtual ComponentInfo* getComponentInf(const char* componentID)  = 0;  // 按ID查询组件信息
};

/** IObjectArray / IObjectManager 在 interface.h（供 pcx.h 内联使用）。 */

/**
 * @brief 日志管理接口（内核使用）
 */
class ILoggerManager
{
public:
	virtual ILogger* getLogger(const char* logName)                                                            = 0;  // 获取指定名称日志器
	virtual ILogger* getDefaultLogger()                                                                        = 0;  // 获取默认日志器
	virtual bool setDefaultLoggerLevel(PcxLogLevel level)                                                      = 0;  // 设置默认日志级别
	virtual bool setDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr)               = 0;  // 设置默认日志输出
	virtual bool addDefaultLogger(ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr)               = 0;  // 添加默认日志输出
	virtual bool setLoggerLevel(const char* logName, PcxLogLevel level)                                        = 0;  // 设置指定日志级别
	virtual bool setLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr) = 0;  // 设置指定日志输出
	virtual bool addLogger(const char* logName, ILoggerWrite* loggerWrite, ILoggerFormat* logFormat = nullptr) = 0;  // 添加指定日志输出
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

PCX_END_NAMESPACE
