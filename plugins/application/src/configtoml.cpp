#include "configtoml.h"
#include "cpptoml.h"
#include <cstdio>
#include <cstring>

namespace pch {

/**
 * @brief Parse log level string to enum
 * @param val Log level string (TRACE/DEBUG/INFO/WARN/ERROR/FATAL/CLOSE)
 * @return Corresponding LogLevel enum value
 */
LogLevel parseLogLevel(const std::string& val)
{
	if (val == "TRACE") return LogLevel::Trace;
	if (val == "DEBUG") return LogLevel::Debug;
	if (val == "INFO") return LogLevel::Information;
	if (val == "WARN") return LogLevel::Warning;
	if (val == "ERROR") return LogLevel::Error;
	if (val == "FATAL") return LogLevel::Fatal;
	if (val == "CLOSE" || val == "OFF") return LogLevel::Off;
	return LogLevel::Debug;
}

/**
 * @brief Constructor
 * @param configPath Path to the TOML configuration file
 */
ConfigToml::ConfigToml(const char* configPath) : ConfigBase(configPath)
{
}

/**
 * @brief Load and parse the TOML configuration file
 * @param pluginsKey Key name for plugins array, defaults to "plugins"
 * @return true on success, false on failure
 */
bool ConfigToml::load(const char* pluginsKey)
{
	// Call base class load to check if already loaded
	if (!ConfigBase::load(pluginsKey))
		return true;

	// Use default key if pluginsKey is invalid or empty
	if (pluginsKey == NULL || strlen(pluginsKey) <= 1)
		pluginsKey = "plugins";

	// Parse the TOML file
	try
	{
		// Parse the TOML file
		_root = cpptoml::parse_file(_configPath);
		/** Read application name from config */
		auto appNameObj = _root->get_as<std::string>("applictionName");
		/** Set application name if present */
		if (appNameObj)
			_applictionName = *appNameObj;

		/** Read plugins list from config */
		std::shared_ptr<cpptoml::array> pPcxPlugins = _root->get_array(pluginsKey);

		/** Iterate plugins array if present */
		if (pPcxPlugins)
		{
			for (const auto& val : *pPcxPlugins)
			{
				auto p = val->as<std::string>();
				if (p)
					_plugins.push_back(p->get());
			}
		}

		/** Read objects table array from config */
		std::shared_ptr<cpptoml::table_array> pPcxObjects = _root->get_table_array_qualified("objects.object");

		/** Iterate objects table array if present */
		if (pPcxObjects)
		{
			for (const auto& val : *pPcxObjects)
			{
				/** Get current object table */
				std::shared_ptr<cpptoml::table> obj = val->as_table();
				if (!obj)
					continue;
				/** 声明对象配置 */
				ObjectCfg objCfg;
				/** 读取组件 ID */
				if (auto cid = obj->get_as<std::string>("componentID"))
					objCfg._componentID = *cid;
				/** 读取对象名称 */
				if (auto objName = obj->get_as<std::string>("objectName"))
					objCfg._objectName = *objName;
				/** 读取初始化参数 */
				if (auto initParm = obj->get_as<std::string>("initParm"))
					objCfg._initParm = *initParm;
				/** 添加到对象列表 */
				_objects.push_back(objCfg);
			}
		}

		/** 读取日志默认配置 */
		std::shared_ptr<cpptoml::table> pLogDefault = _root->get_table_qualified("log.default");

		/** 如果默认日志配置存在 */
		if (pLogDefault)
		{
			std::shared_ptr<cpptoml::table> t = pLogDefault->as_table();
			if (t)
			{
				/** 读取日志级别 */
				if (auto level = t->get_as<std::string>("level"))
					_defaultLogLevel = parseLogLevel(*level);
			}
		}

		/** 读取默认日志级别（备用） */
		auto default_level = _root->get_qualified_as<std::string>("log.default_level");
		/** 如果备用配置存在 */
		if (default_level)
			/** 设置默认日志级别 */
			_defaultLogLevel = parseLogLevel(*default_level);
	}
	// 解析异常
	catch (const cpptoml::parse_exception& e)
	{
		printf("%s load err! %s\n", _configPath.c_str(), e.what());
		// 返回失败
		return false;
	}
	// 返回成功
	return true;
}

}
