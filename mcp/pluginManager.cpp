#include <string.h>
#include "pluginManager.h"
#include "loggerManager.h"
#include "componentManager.h"
#include "error.h"

MCP_REGISTER_COMPONENT(mcp::PluginManager, MCP_PLUGIN_MANAGER_ID)

MCP_BEGIN_NAMESPACE


extern ComponentManager* _componentManager;	//全局组件管理器实例指针，由mcp初始化流程维护。

/**
 * @brief 析构插件管理器并释放所有已加载插件对象。
 * @details
 * 释放所有已加载插件对象，并清空插件映射表。
 */
PluginManager::~PluginManager()
{
	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 遍历插件映射表，释放每个插件对象
	for (auto it = _plugins.begin(); it != _plugins.end(); it++) {
		if (it->second != nullptr) {
			delete it->second;
		}
	}

	// 清空插件映射表
	_plugins.clear();
}

/**
 * @brief 按路径加载插件并完成注册。
 * @param[in] path 插件动态库路径。
 * @return 成功返回插件对象指针，失败返回 nullptr。
 */
IPlugin* PluginManager::loadPlugin(const char* path)
{
	// 如果路径为空，则返回失败
	if (path == nullptr || strcmp(path, "") == 0){
		WriteLog(McpLogLevel::Debug, "Load plugin failed, path is nullptr or empty");
		return nullptr;
	}

	// 加载插件
	Plugin* pPlugin = Plugin::load(path);

	// 如果加载成功，则注册插件
	if (pPlugin)
	{
		if (registerPlugin(pPlugin) == MCP_SUCCESS)
		{
			WriteLog(McpLogLevel::Debug, "Load plugin [%s][%s] succeed", pPlugin->getName(), path);
		}
		else
		{
			WriteLog(McpLogLevel::Debug, "Load plugin [%s][%s] failed", pPlugin->getName(), path);
		}
	}
	return pPlugin;
}

/**
 * @brief 注册插件并导入其组件信息表到组件管理器。
 * @param[in] plugin 插件对象指针。
 * @return 注册结果错误码。
 */
ErrorCode PluginManager::registerPlugin(Plugin* plugin)
{
	// 如果插件对象为空，则返回失败
	if (plugin == nullptr)
		return MCP_PARAM_NULLPTR;

	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 如果插件名称不存在，则注册插件
	if (_plugins.find(plugin->getName()) == _plugins.end()) {
		_plugins[plugin->getName()] = plugin;

		// 如果组件管理器可用，则注册组件
		if (_componentManager) {
			ComponentInfo** comTable = plugin->getComponentInfos();
			if (comTable == nullptr) {
				return MCP_COMPONENT_INVALID;
			}
			for (int j = 0; comTable[j] != nullptr; j++) {
				_componentManager->registerComponent(comTable[j]);
			}
		} 
		else {
			WriteLog(McpLogLevel::Warning, "MCP has not been initialized! can't register component!");
			return MCP_COMPONENT_NULLPTR;
		}
		return MCP_SUCCESS;
	} 
	else {
		// 如果插件名称已存在，则返回失败
		WriteLog(McpLogLevel::Warning, "plugin name[%s] exist!", plugin->getName());
		return MCP_PLUGIN_ADYEXIST;
	}
	return MCP_SUCCESS;
}

/**
 * @brief 卸载插件。
 * @param[in] plugin 插件对象指针。
 * @return 卸载结果错误码。
 */
ErrorCode PluginManager::unloadPlugin(IPlugin* plugin)
{
	return MCP_SUCCESS;
}

/**
 * @brief 查找插件对象。
 * @param[in] pluginName 插件名称。
 * @return 命中返回插件对象指针，未命中返回 nullptr。
 */
Plugin* PluginManager::findPlugin(const char* pluginName)
{
	// 如果插件名称为空，则返回失败
	if (pluginName == nullptr || strcmp(pluginName, "") == 0) {
		WriteLog(McpLogLevel::Debug, "FindPlugin: pluginName empty!");
		return nullptr;
	}

	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);
	// 查找插件
	auto it = _plugins.find(pluginName);
	// 如果插件存在，则返回插件对象
	if (it != _plugins.end()) {
		return it->second;
	} 
	else {
		return nullptr;
	}
}

/**
 * @brief 获取已加载插件名称列表。
 * @return 已加载插件名称列表。
 */
std::list<std::string> PluginManager::getLoadedPlugins()
{
	std::list<std::string> pluginNames;
	// 加锁，防止多线程同时访问	
	std::lock_guard<std::mutex> lk(_mutex);
	// 遍历插件映射表，获取插件名称
	for (auto it = _plugins.begin(); it != _plugins.end(); it++) {
		pluginNames.push_back(it->first);
	}
	return pluginNames;
}

MCP_END_NAMESPACE
