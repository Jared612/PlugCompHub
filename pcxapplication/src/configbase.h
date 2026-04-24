 /**
 * @file configbase.h
 * @brief 应用配置抽象基类：插件列表、预创建对象、默认日志级别等（由 ConfigToml 等填充）。
 */
#pragma once

#include "interface.h"
#include <string>
#include <vector>

namespace pcx {

/** 预创建对象配置 */
struct ObjectCfg
{
	std::string _componentID;  // 组件 ID（CreateNamedObject）
	std::string _objectName;   // 具名对象名
	std::string _initParm;     // 初始化参数（可与 SystemObjectInit 配合）
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
	 * @param pluginsKey 插件键
	 * @return 是否成功
	 */
	virtual bool load(const char* pluginsKey = "plugins")
	{
		(void)pluginsKey;
		/** 如果已加载，则返回 false */
		if (_isLoad)
			return false;
		/** 设置已加载 */
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
	 * @brief 获取预创建对象列表
	 * @return 预创建对象列表
	 */
	std::vector<ObjectCfg>& getObjects() { return _objects; }

	/**
	 * @brief 获取默认日志级别
	 * @return 默认日志级别
	 */
	PcxLogLevel getDefaultLogLevel() const { return _defaultLogLevel; }

protected:
	std::string              _configPath;                            // 配置文件路径
	std::string              _applictionName;                        // 应用名称
	std::vector<std::string> _plugins;                               // 插件列表
	std::vector<ObjectCfg>   _objects;                               // 预创建对象列表
	PcxLogLevel              _defaultLogLevel = PcxLogLevel::Trace;  // 默认日志级别
	bool                     _isLoad = false;                        // 是否已加载
};

}
