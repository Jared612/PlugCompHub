/**
 * @file pluginManager.h
 * @brief 插件管理器：加载和注册 Plugin，并将插件组件表交给 ComponentManager
 * @details 实现在 pluginManager.cpp 中；接口为 IPluginManager
 */
#pragma once
#include "interface.h"
#include "plugin.h"
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

PCH_BEGIN_NAMESPACE

/**
 * @class PluginManager
 * @brief 实现 IPluginManager，负责加载/注册插件并将组件表同步到 ComponentManager
 */
class PluginManager: public IPluginManager, public IMessageHandler
{
	/** @brief 插件名称 -> Plugin* 映射表 */
	using PluginMap = std::unordered_map<std::string, Plugin*>;

public:
	/** @brief 默认构造函数 */
	PluginManager() = default;

	/** @brief 析构函数，释放所有已加载的插件对象 */
	virtual ~PluginManager();

	/**
	 * @brief 按路径加载插件并注册到插件管理器
	 * @param[in] path 插件动态库路径
	 * @return 成功返回插件对象指针，失败返回 nullptr
	 */
	IPlugin* loadPlugin(const char* path);

	/**
	 * @brief 注册插件并将组件信息表导入组件管理器
	 * @param[in] plugin 插件对象指针
	 * @return 注册结果错误码
	 */
	ErrorCode registerPlugin(Plugin* plugin);

	/**
	 * @brief 卸载插件（当前实现为空）
	 * @param[in] plugin 插件对象指针
	 * @return 结果错误码
	 */
	ErrorCode unloadPlugin(IPlugin* plugin);

	/**
	 * @brief 按插件名称查找插件
	 * @param[in] pluginName 插件名
	 * @return 命中返回插件对象指针，未命中返回 nullptr
	 */
	Plugin* findPlugin(const char* pluginName);

	/**
	 * @brief 获取当前已加载的插件名称列表
	 * @return 已加载插件名称列表
	 */
	std::list<std::string> getLoadedPlugins();

	/**
	 * @brief 处理系统广播消息
	 * @details 当前仅处理 SystemShutdown：遍历通知已加载插件执行 pluginexit，
	 *          清除插件侧 pch::api::_objectManager，避免主机销毁后插件悬空引用
	 */
	const pch::IMessage* handleMessage(const pch::IMessage* msg) override;

private:
	/**
	 * @brief 检查插件导出的组件是否仍有活跃对象在 ObjectManager 中
	 * @param[in] plugin 插件对象指针
	 * @return 存在活跃对象返回 true；否则返回 false
	 */
	bool hasLiveObjectsOfPlugin(Plugin* plugin);

private:
	std::mutex _mutex;    // 保护 _plugins 的互斥锁
	PluginMap  _plugins;  // 已加载插件表
};

PCH_END_NAMESPACE
