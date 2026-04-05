/**
 * @file pluginManager.h
 * @brief 插件管理器：加载与注册 `Plugin`，并把插件组件表交给 `ComponentManager`。
 * @details 实现见 `pluginManager.cpp`；接口为 `IPluginManager`	。
 */
#pragma once
#include "interface.h"
#include "internal.h"
#include "plugin.h"
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

MCP_BEGIN_NAMESPACE

/**
 * @class PluginManager	
 * @brief 实现 `IPluginManager`，负责加载/注册插件并同步组件表到 `ComponentManager`。
 */
class PluginManager: public IPluginManager
{
	/** @brief 插件名 → Plugin* 的映射表。 */
	using PluginMap = std::unordered_map<std::string, Plugin*>;

public:
	/** @brief 默认构造函数。 */
	PluginManager() = default;

	/** @brief 析构函数，释放所有已加载插件对象。 */
	virtual ~PluginManager();

	/**
	 * @brief 按路径加载插件并注册到插件管理器。
	 * @param[in] path 插件动态库路径。
	 * @return 成功返回插件对象指针，失败返回 nullptr。
	 */
	IPlugin* loadPlugin(const char* path);

	/**
	 * @brief 注册插件并导入组件信息表到组件管理器。
	 * @param[in] plugin 插件对象指针。
	 * @return 注册结果错误码。
	 */
	ErrorCode registerPlugin(Plugin* plugin);

	/**
	 * @brief 卸载插件（当前实现为空）。
	 * @param[in] plugin 插件对象指针。
	 * @return 结果错误码。
	 */
	ErrorCode unloadPlugin(IPlugin* plugin);

	/**
	 * @brief 按插件名称查找插件。
	 * @param[in] pluginName 插件名称。
	 * @return 命中返回插件对象指针，未命中返回 nullptr。
	 */
	Plugin* findPlugin(const char* pluginName);

	/**
	 * @brief 获取当前已加载插件名称列表。
	 * @return 已加载插件名称列表。
	 */
	std::list<std::string> getLoadedPlugins();

private:
	std::mutex _mutex;    //保护 _plugins 的互斥锁。
	PluginMap  _plugins;  //已加载插件表。
};

MCP_END_NAMESPACE