#include <string.h>
#include <unordered_set>
#include "pluginManager.h"
#include "loggerManager.h"
#include "componentManager.h"
#include "objectManager.h"
#include "error.h"

PCX_REGISTER_COMPONENT(pcx::PluginManager, PCX_PLUGIN_MANAGER_ID)

PCX_BEGIN_NAMESPACE


extern ComponentManager* _componentManager;	//全局组件管理器实例指针，由PCX初始化流程维护。
extern ObjectManager*    _objManager;       //全局对象管理器实例指针，由PCX初始化流程维护。

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
	// 流程：LoadLibrary/dlopen → Plugin::load 内解析 pluginfo/pluginit → registerPlugin 把组件表注册进 ComponentManager
	if (path == nullptr || strcmp(path, "") == 0) {
		WriteLog(PcxLogLevel::Debug, "Load plugin failed, path is nullptr or empty");
		return nullptr;
	}

	Plugin* pPlugin = Plugin::load(path);
	if (pPlugin == nullptr) {
		return nullptr;
	}

	ErrorCode ec = registerPlugin(pPlugin);
	if (ec == PCX_SUCCESS) {
		WriteLog(PcxLogLevel::Debug, "Load plugin [%s][%s] succeed", pPlugin->getName(), path);
		return pPlugin;
	}

	WriteLog(PcxLogLevel::Warning, "Load plugin [%s][%s] failed, errCode=%d; unloading",
			 pPlugin->getName(), path, (int)ec);
	// 注册失败不得把半初始化的插件暴露给调用方；释放已加载的 DLL 资源避免泄漏
	delete pPlugin;
	return nullptr;
}

/**
 * @brief 注册插件并导入其组件信息表到组件管理器。
 * @param[in] plugin 插件对象指针。
 * @return 注册结果错误码。
 * @note 先做所有前置校验再写 `_plugins`；任一校验失败不留下半注册状态。
 */
ErrorCode PluginManager::registerPlugin(Plugin* plugin)
{
	// 如果插件对象为空，则返回失败
	if (plugin == nullptr)
		return PCX_PARAM_NULLPTR;

	const char* pluginName = plugin->getName();
	if (pluginName == nullptr || pluginName[0] == '\0') {
		WriteLog(PcxLogLevel::Warning, "registerPlugin: plugin name is empty");
		return PCX_PLUGIN_NULLPTR;
	}

	// 先校验：组件管理器必须就绪
	if (_componentManager == nullptr) {
		WriteLog(PcxLogLevel::Warning, "PCX has not been initialized! can't register component!");
		return PCX_COMMANAGER_NULLPTR;
	}

	// 先校验：插件必须导出有效组件表
	ComponentInfo** comTable = plugin->getComponentInfos();
	if (comTable == nullptr) {
		WriteLog(PcxLogLevel::Warning, "plugin[%s] has no component info table", pluginName);
		return PCX_COMPONENT_INVALID;
	}

	// 加锁后写 _plugins；重名直接失败，不触碰组件管理器
	{
		std::lock_guard<std::mutex> lk(_mutex);
		if (_plugins.find(pluginName) != _plugins.end()) {
			WriteLog(PcxLogLevel::Warning, "plugin name[%s] exist!", pluginName);
			return PCX_PLUGIN_ADYEXIST;
		}
		_plugins[pluginName] = plugin;
	}

	// 组件逐项注册：单个失败仅告警不回滚整体（与 `ComponentManager::registerComponents` 保持一致）
	for (int j = 0; comTable[j] != nullptr; j++) {
		ErrorCode r = _componentManager->registerComponent(comTable[j]);
		if (r != PCX_SUCCESS) {
			WriteLog(PcxLogLevel::Warning, "plugin[%s] register component[%s] failed, errCode=%d",
					 pluginName,
					 comTable[j] && comTable[j]->componentID ? comTable[j]->componentID : "(null)",
					 (int)r);
		}
	}
	return PCX_SUCCESS;
}

/**
 * @brief 卸载插件：存在活对象时拒绝，否则注销其组件、移除映射并销毁 Plugin（关闭动态库）。
 * @param[in] plugin 插件对象指针。
 * @return 卸载结果错误码。
 */
ErrorCode PluginManager::unloadPlugin(IPlugin* plugin)
{
	if (plugin == nullptr)
		return PCX_PARAM_NULLPTR;

	Plugin* self = dynamic_cast<Plugin*>(plugin);
	if (self == nullptr) {
		WriteLog(PcxLogLevel::Warning, "unloadPlugin: plugin is not a Plugin instance");
		return PCX_PARAM_INVALID;
	}

	const char* pluginName = self->getName();
	if (pluginName == nullptr || pluginName[0] == '\0') {
		WriteLog(PcxLogLevel::Warning, "unloadPlugin: plugin name is empty");
		return PCX_PLUGIN_NULLPTR;
	}

	// 先 unregister 组件（使 `findComp` 立即失败，阻断新 `createObject`），再判活对象，最后清 `_plugins`、卸 DLL。
	// 全程持 PluginManager `_mutex`，同时阻断 `registerPlugin`/重名加载；但由于 `ComponentManager`/`ObjectManager`
	// 各自独立锁，"另一个线程在 unregister 之前刚拿到 `Component*` 并即将 createObject" 的极窄窗口无法在本层彻底消除，
	// 此类并发应由上层串行化 unload 与业务对象创建。
	std::lock_guard<std::mutex> lk(_mutex);

	auto it = _plugins.find(pluginName);
	if (it == _plugins.end() || it->second != self) {
		return PCX_PLUGIN_NOTFOUND;
	}

	ComponentInfo** comTable = self->getComponentInfos();

	// 1) 先注销组件，阻断后续 createObject 再经由本插件的 Component* 创建新对象
	if (_componentManager != nullptr && comTable != nullptr) {
		for (int j = 0; comTable[j] != nullptr; j++) {
			_componentManager->unregisterComponent(comTable[j]);
		}
	}

	// 2) 再判活对象；若仍有存活对象，回滚注销并拒绝卸载（DLL 里 deletor 失效会造成 UAF）
	if (hasLiveObjectsOfPlugin(self)) {
		WriteLog(PcxLogLevel::Warning,
				 "unloadPlugin: plugin[%s] still has live objects, refuse to unload", pluginName);
		if (_componentManager != nullptr && comTable != nullptr) {
			for (int j = 0; comTable[j] != nullptr; j++) {
				// 优先从延迟删除队列里复用原 `Component*`，避免新 `new` 一份并把旧包装永久遗留
				ErrorCode rr = _componentManager->reregisterFromDeleteList(comTable[j]);
				if (rr == PCX_COMPONENT_NOTFOUND) {
					_componentManager->registerComponent(comTable[j]);
				}
			}
		}
		return PCX_NOTALLOW;
	}

	// 3) 从 _plugins 移除并销毁 Plugin（析构关闭动态库）
	_plugins.erase(it);
	delete self;
	WriteLog(PcxLogLevel::Debug, "Plugin [%s] unloaded", pluginName);
	return PCX_SUCCESS;
}

/**
 * @brief 判断插件组件表中的任一 `ComponentInfo*` 是否仍在 `ObjectManager` 中有活对象。
 */
bool PluginManager::hasLiveObjectsOfPlugin(Plugin* plugin)
{
	if (plugin == nullptr || _objManager == nullptr)
		return false;

	ComponentInfo** comTable = plugin->getComponentInfos();
	if (comTable == nullptr)
		return false;

	// 构造本插件组件表的指针集合，O(1) 判定 ObjectInfo 归属
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
 * @brief 查找插件对象。
 * @param[in] pluginName 插件名称。
 * @return 命中返回插件对象指针，未命中返回 nullptr。
 */
Plugin* PluginManager::findPlugin(const char* pluginName)
{
	// 如果插件名称为空，则返回失败
	if (pluginName == nullptr || strcmp(pluginName, "") == 0) {
		WriteLog(PcxLogLevel::Debug, "FindPlugin: pluginName empty!");
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

/**
 * @brief 处理系统广播消息（当前只关心 `SystemShutdown`）。
 */
const pcx::IMessage* PluginManager::handleMessage(const pcx::IMessage* msg)
{
	if (msg == nullptr) {
		return nullptr;
	}
	if (msg->getCode() != pcx::SystemShutdown) {
		return nullptr;
	}

	// 广播 `SystemShutdown` 时，逐个清理插件侧 `pcx::api::_objectManager`，
	// 避免宿主销毁 ObjectManager 后插件仍持有悬挂指针。
	std::lock_guard<std::mutex> lk(_mutex);
	for (auto it = _plugins.begin(); it != _plugins.end(); ++it) {
		if (it->second != nullptr) {
			it->second->exitPluginAPI();
		}
	}
	return nullptr;
}

PCX_END_NAMESPACE
