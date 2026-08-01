#include <string.h>
#include <unordered_set>
#include "pluginManager.h"
#include "loggerManager.h"
#include "componentManager.h"
#include "objectManager.h"
#include "error.h"

PCH_REGISTER_COMPONENT(pch::PluginManager, PCH_PLUGIN_MANAGER_ID)

PCH_BEGIN_NAMESPACE


extern ComponentManager* _componentManager;	// 全局组件管理器实例指针，由 PCH 初始化流程维护
extern ObjectManager*    _objManager;       // 全局对象管理器实例指针，由 PCH 初始化流程维护

/**
 * @brief 析构插件管理器并释放所有已加载的插件对象
 * @details
 * 释放所有已加载的插件对象并清空插件映射表
 */
PluginManager::~PluginManager()
{
	// 加锁防止并发访问
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
 * @brief 按路径加载插件并完成注册
 * @param[in] path 插件动态库路径
 * @return 成功返回插件对象指针，失败返回 nullptr
 */
IPlugin* PluginManager::loadPlugin(const char* path)
{
	// 流程：LoadLibrary/dlopen -> Plugin::load 解析 pluginfo/pluginit -> registerPlugin 向 ComponentManager 注册组件表
	if (path == nullptr || strcmp(path, "") == 0) {
		WriteLog(LogLevel::Debug, "Load plugin failed, path is nullptr or empty");
		return nullptr;
	}

	Plugin* pPlugin = Plugin::load(path);
	if (pPlugin == nullptr) {
		return nullptr;
	}

	ErrorCode ec = registerPlugin(pPlugin);
	if (ec == PCH_SUCCESS) {
		WriteLog(LogLevel::Debug, "Load plugin [%s][%s] succeed", pPlugin->getName(), path);
		return pPlugin;
	}

	WriteLog(LogLevel::Warning, "Load plugin [%s][%s] failed, errCode=%d; unloading",
			 pPlugin->getName(), path, (int)ec);
	// 注册失败不得将半初始化的插件暴露给调用方；释放已加载的 DLL 资源，避免泄漏
	delete pPlugin;
	return nullptr;
}

/**
 * @brief 注册插件并将其组件信息表导入组件管理器
 * @param[in] plugin 插件对象指针
 * @return 注册结果错误码
 * @note 先完成所有前置检查再写入 _plugins；任一检查失败不会留下半注册状态
 */
ErrorCode PluginManager::registerPlugin(Plugin* plugin)
{
	// 如果插件对象为空，返回失败
	if (plugin == nullptr)
		return PCH_PARAM_NULLPTR;

	const char* pluginName = plugin->getName();
	if (pluginName == nullptr || pluginName[0] == '\0') {
		WriteLog(LogLevel::Warning, "registerPlugin: plugin name is empty");
		return PCH_PLUGIN_NULLPTR;
	}

	// 前置检查：组件管理器必须就绪
	if (_componentManager == nullptr) {
		WriteLog(LogLevel::Warning, "PCH has not been initialized! can't register component!");
		return PCH_COMMANAGER_NULLPTR;
	}

	// 前置检查：插件必须导出有效的组件表
	ComponentInfo** comTable = plugin->getComponentInfos();
	if (comTable == nullptr) {
		WriteLog(LogLevel::Warning, "plugin[%s] has no component info table", pluginName);
		return PCH_COMPONENT_INVALID;
	}

	// 加锁后写入 _plugins；名称重复立即失败，不触碰组件管理器
	{
		std::lock_guard<std::mutex> lk(_mutex);
		if (_plugins.find(pluginName) != _plugins.end()) {
			WriteLog(LogLevel::Warning, "plugin name[%s] exist!", pluginName);
			return PCH_PLUGIN_ADYEXIST;
		}
		_plugins[pluginName] = plugin;
	}

	// 逐个组件注册：单个失败仅警告，不整体回滚（与 ComponentManager::registerComponents 一致）
	for (int j = 0; comTable[j] != nullptr; j++) {
		ErrorCode r = _componentManager->registerComponent(comTable[j]);
		if (r != PCH_SUCCESS) {
			WriteLog(LogLevel::Warning, "plugin[%s] register component[%s] failed, errCode=%d",
					 pluginName,
					 comTable[j] && comTable[j]->componentID ? comTable[j]->componentID : "(null)",
					 (int)r);
		}
	}
	return PCH_SUCCESS;
}

/**
 * @brief 卸载插件：有活跃对象时拒绝卸载，否则注销其组件、移除映射并销毁 Plugin（关闭动态库）
 * @param[in] plugin 插件对象指针
 * @return 结果错误码
 */
ErrorCode PluginManager::unloadPlugin(IPlugin* plugin)
{
	if (plugin == nullptr)
		return PCH_PARAM_NULLPTR;

	Plugin* self = dynamic_cast<Plugin*>(plugin);
	if (self == nullptr) {
		WriteLog(LogLevel::Warning, "unloadPlugin: plugin is not a Plugin instance");
		return PCH_PARAM_INVALID;
	}

	const char* pluginName = self->getName();
	if (pluginName == nullptr || pluginName[0] == '\0') {
		WriteLog(LogLevel::Warning, "unloadPlugin: plugin name is empty");
		return PCH_PLUGIN_NULLPTR;
	}

	// 先注销组件（使 findComp 立即失败，阻止新的 createObject），再检查活跃对象，最后清空 _plugins，卸载 DLL。
	// 全程持有 PluginManager _mutex，也阻止了 registerPlugin/同名加载；但由于 ComponentManager/ObjectManager
	// 各有独立锁，"另一线程在注销前刚拿到 Component* 并立即 createObject" 的极窄窗口在此层无法完全消弭。
	// 此类并发应由上层序列化卸载与业务对象创建来处理。
	std::lock_guard<std::mutex> lk(_mutex);

	auto it = _plugins.find(pluginName);
	if (it == _plugins.end() || it->second != self) {
		return PCH_PLUGIN_NOTFOUND;
	}

	ComponentInfo** comTable = self->getComponentInfos();

	// 1) 先注销组件，阻止后续 createObject 通过此插件的 Component* 创建新对象
	if (_componentManager != nullptr && comTable != nullptr) {
		for (int j = 0; comTable[j] != nullptr; j++) {
			_componentManager->unregisterComponent(comTable[j]);
		}
	}

	// 2) 然后检查活跃对象；如果仍有活跃对象，回滚注销并拒绝卸载（DLL 的 deletor 失效会导致 UAF）
	if (hasLiveObjectsOfPlugin(self)) {
		WriteLog(LogLevel::Warning,
				 "unloadPlugin: plugin[%s] still has live objects, refuse to unload", pluginName);
		if (_componentManager != nullptr && comTable != nullptr) {
			for (int j = 0; comTable[j] != nullptr; j++) {
				// 优先从延迟删除队列中重用 Component*，避免重新 new 一个而旧 wrapper 永远留在队列中
				ErrorCode rr = _componentManager->reregisterFromDeleteList(comTable[j]);
				if (rr == PCH_COMPONENT_NOTFOUND) {
					_componentManager->registerComponent(comTable[j]);
				}
			}
		}
		return PCH_NOTALLOW;
	}

	// 3) 从 _plugins 中移除并销毁 Plugin（析构函数关闭动态库）
	_plugins.erase(it);
	delete self;
	WriteLog(LogLevel::Debug, "Plugin [%s] unloaded", pluginName);
	return PCH_SUCCESS;
}

/**
 * @brief 检查插件组件表中的任意 ComponentInfo* 是否仍在 ObjectManager 中有活跃对象
 */
bool PluginManager::hasLiveObjectsOfPlugin(Plugin* plugin)
{
	if (plugin == nullptr || _objManager == nullptr)
		return false;

	ComponentInfo** comTable = plugin->getComponentInfos();
	if (comTable == nullptr)
		return false;

	// 构建此插件组件表的指针集合，O(1) 的 ObjectInfo 所有权检查
	std::unordered_set<ComponentInfo*> own;
	for (int j = 0; comTable[j] != nullptr; j++) {
		own.insert(comTable[j]);
	}

	auto objs = _objManager->getRegisterObjects();
	for (auto* oi : objs) {
		if (oi == nullptr || oi->component == nullptr)
			continue;
		if (own.count(oi->component->getComponentInfo()) > 0) {
			return true;
		}
	}
	return false;
}

/**
 * @brief 查找插件对象
 * @param[in] pluginName 插件名
 * @return 命中返回插件对象指针，未命中返回 nullptr
 */
Plugin* PluginManager::findPlugin(const char* pluginName)
{
	// 如果插件名为空，返回失败
	if (pluginName == nullptr || strcmp(pluginName, "") == 0) {
		WriteLog(LogLevel::Debug, "FindPlugin: pluginName empty!");
		return nullptr;
	}

	// 加锁防止并发访问
	std::lock_guard<std::mutex> lk(_mutex);
	// 查找插件
	auto it = _plugins.find(pluginName);
	// 如果插件存在，返回插件对象
	if (it != _plugins.end()) {
		return it->second;
	} 
	else {
		return nullptr;
	}
}

/**
 * @brief 获取已加载的插件名称列表
 * @return 已加载插件名称列表
 */
std::list<std::string> PluginManager::getLoadedPlugins()
{
	std::list<std::string> pluginNames;
	// 加锁防止并发访问	
	std::lock_guard<std::mutex> lk(_mutex);
	// 遍历插件映射表，获取插件名称
	for (auto it = _plugins.begin(); it != _plugins.end(); it++) {
		pluginNames.push_back(it->first);
	}
	return pluginNames;
}

/**
 * @brief 处理系统广播消息（当前仅关注 SystemShutdown）
 */
const pch::IMessage* PluginManager::handleMessage(const pch::IMessage* msg)
{
	if (msg == nullptr) {
		return nullptr;
	}
	if (msg->getCode() != pch::SystemShutdown) {
		return nullptr;
	}

	// 广播 SystemShutdown 时，遍历清理插件侧 pch::api::_objectManager。
	// 避免插件在主机销毁 ObjectManager 后持有悬空指针。
	std::lock_guard<std::mutex> lk(_mutex);
	for (auto it = _plugins.begin(); it != _plugins.end(); ++it) {
		if (it->second != nullptr) {
			it->second->exitPluginAPI();
		}
	}
	return nullptr;
}

PCH_END_NAMESPACE
