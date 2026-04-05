#include "configtoml.h"
#include "cpptoml.h"
#include <cstdio>
#include <cstring>

namespace mcp {

/**
 * @brief 解析日志级别
 * @param val 日志级别字符串
 * @return 日志级别
 */
McpLogLevel parseLogLevel(const std::string& val)
{
	if (val == "TRACE") return McpLogLevel::Trace;
	if (val == "DEBUG") return McpLogLevel::Debug;
	if (val == "INFO") return McpLogLevel::Information;
	if (val == "WARN") return McpLogLevel::Warning;
	if (val == "ERROR") return McpLogLevel::Error;
	if (val == "FATAL") return McpLogLevel::Fatal;
	if (val == "CLOSE" || val == "OFF") return McpLogLevel::Off;
	return McpLogLevel::Debug;
}

/**
 * @brief 构造函数
 * @param configPath 配置文件路径
 */
ConfigToml::ConfigToml(const char* configPath) : ConfigBase(configPath)
{
}

/**
 * @brief 加载配置
 * @param pluginsKey 插件键，默认 "plugins"
 * @return 是否成功
 */
bool ConfigToml::load(const char* pluginsKey)
{
	// 如果未加载，则加载配置文件
	if (!ConfigBase::load(pluginsKey))
		return true;

	// 如果插件键为空，则使用默认插件键
	if (pluginsKey == NULL || strlen(pluginsKey) <= 1)
		pluginsKey = "plugins";

	// 解析配置文件
	try
	{
		/** 解析配置文件 */
		_root = cpptoml::parse_file(_configPath);
		/** 获取应用名称 */
		auto appNameObj = _root->get_as<std::string>("applictionName");
		/** 如果应用名称不为空，则设置应用名称 */
		if (appNameObj)
			_applictionName = *appNameObj;

		/** 获取插件列表 */
		std::shared_ptr<cpptoml::array> pMcpPlugins = _root->get_array(pluginsKey);

		/** 如果插件列表不为空，则遍历插件列表，获取插件名称并添加到插件列表 */
		if (pMcpPlugins)
		{
			for (const auto& val : *pMcpPlugins)
			{
				auto p = val->as<std::string>();
				if (p)
					_plugins.push_back(p->get());
			}
		}

		/** 获取预创建对象列表 */
		std::shared_ptr<cpptoml::table_array> pMcpObjects = _root->get_table_array_qualified("objects.object");

		/** 如果预创建对象列表不为空，则遍历预创建对象列表，获取对象名称并添加到预创建对象列表 */
		if (pMcpObjects)
		{
			for (const auto& val : *pMcpObjects)
			{
				/** 获取对象 */
				std::shared_ptr<cpptoml::table> obj = val->as_table();
				if (!obj)
					continue;
				/** 创建对象配置 */
				ObjectCfg objCfg;
				/** 获取组件 ID */
				if (auto cid = obj->get_as<std::string>("componentID"))
					objCfg._componentID = *cid;
				/** 获取对象名称 */
				if (auto objName = obj->get_as<std::string>("objectName"))
					objCfg._objectName = *objName;
				/** 获取初始化参数 */
				if (auto initParm = obj->get_as<std::string>("initParm"))
					objCfg._initParm = *initParm;
				/** 添加到预创建对象列表 */
				_objects.push_back(objCfg);
			}
		}

		/** 获取默认日志配置 */
		std::shared_ptr<cpptoml::table> pLogDefault = _root->get_table_qualified("log.default");

		/** 如果默认日志配置不为空，则获取默认日志级别 */
		if (pLogDefault)
		{
			std::shared_ptr<cpptoml::table> t = pLogDefault->as_table();
			if (t)
			{
				/** 获取默认日志级别 */
				if (auto level = t->get_as<std::string>("level"))
					_defaultLogLevel = parseLogLevel(*level);
			}
		}

		/** 获取默认日志级别 */
		auto default_level = _root->get_qualified_as<std::string>("log.default_level");
		/** 如果默认日志级别不为空，则获取默认日志级别 */
		if (default_level)
			/** 设置默认日志级别 */
			_defaultLogLevel = parseLogLevel(*default_level);
	}
	// 解析配置文件失败
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
