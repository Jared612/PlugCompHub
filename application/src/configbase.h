/**
 * @file configbase.h
 * @brief 配置基类声明，具体实现参见子类 ConfigToml
 */
#pragma once

#include "interface.h"
#include <string>
#include <vector>

namespace pch {

/** @brief 对象配置结构体 */
struct ObjectCfg
{
	std::string _componentID;  // 组件 ID，用于 CreateNamedObject
	std::string _objectName;   // 对象名称
	std::string _initParm;     // 初始化参数，传递给 SystemObjectInit
};

class ConfigBase
{
public:
	/**
	 * @brief 构造函数
	 * @param configPath 配置文件路径
	 */
	explicit ConfigBase(const char* configPath)
		: _configPath(configPath ? configPath : "")
		, _isLoad(false)
	{
	}

	/**
	 * @brief 加载配置
	 * @param pluginsKey 插件键名
	 * @return 是否成功
	 */
	virtual bool load(const char* pluginsKey = "plugins")
	{
		(void)pluginsKey;
		/** 已加载则返回 false */
		if (_isLoad)
			return false;
		/** 标记为已加载 */
		_isLoad = true;
		/** 返回 true */
		return true;
	}

	/**
	 * @brief 获取应用名称
	 * @return 应用名称
	 */
	const char* getApplictionName() { return _applictionName.c_str(); }

	/**
	 * @brief 获取插件列表
	 * @return 插件列表
	 */
	std::vector<std::string> getPlugins() { return _plugins; }

	/**
	 * @brief 获取对象配置列表
	 * @return 对象配置列表
	 */
	std::vector<ObjectCfg>& getObjects() { return _objects; }

	/**
	 * @brief 获取默认日志级别
	 * @return 默认日志级别
	 */
	LogLevel getDefaultLogLevel() const { return _defaultLogLevel; }

protected:
	std::string              _configPath;                            // 配置文件路径
	std::string              _applictionName;                        // 应用名称
	std::vector<std::string> _plugins;                               // 插件列表
	std::vector<ObjectCfg>   _objects;                               // 对象配置列表
	LogLevel                 _defaultLogLevel = LogLevel::Trace;  // 默认日志级别
	bool                     _isLoad = false;                        // 是否已加载
};

}
