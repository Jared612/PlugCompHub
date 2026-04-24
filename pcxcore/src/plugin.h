/**
 * @file plugin.h
 * @brief 单个插件动态库的封装：加载/卸载、解析导出符号与组件表。
 * @details 实现见 `plugin.cpp`；接口为 `IPlugin`。
 */
#pragma once
#include "interface.h"
#include "internal.h"
#include "pcxcomponent.h"
#include "pcxexport.h"
#include "pcxplugin.h"
#include <string>

PCX_BEGIN_NAMESPACE

/**
 * @class Plugin
 * @brief 动态插件封装对象	实现 `IPlugin`，负责加载/卸载插件、解析导出符号与组件表。
 */
class Plugin: public IPlugin
{
public:
	/**
	 * @brief 按路径加载插件。
	 * @param[in] path 插件动态库路径。
	 * @return 加载成功返回 Plugin 对象指针，失败返回 nullptr。
	 */
	static Plugin* load(const char* path);

	/**
	 * @brief 析构插件对象并释放动态库句柄。
	 */
	virtual ~Plugin();

	/**
	 * @brief 获取插件名称。
	 * @return 插件名称字符串。
	 */
	const char* getName();

	/**
	 * @brief 获取插件版本。
	 * @return 插件版本字符串。
	 */
	const char* getVersion();

	/**
	 * @brief 获取插件描述。
	 * @return 插件描述字符串。
	 */
	const char* getDescription();

	/**
	 * @brief 获取插件文件路径。
	 * @return 插件路径字符串。
	 */
	const char* getPath();

	/**
	 * @brief 获取插件导出的组件信息表。
	 * @return 以 nullptr 结尾的组件信息表，失败返回 nullptr。
	 */
	ComponentInfo** getComponentInfos();

	/**
	 * @brief 解析动态库导出符号。
	 * @param[in] symbol 符号名称。
	 * @return 成功返回符号地址，失败返回 nullptr。
	 */
	void* resolve(const char* symbol);

	/**
	 * @brief 调用插件导出的 `pluginexit`（若存在），把插件侧 `pcx::api::_objectManager` 置空。
	 * @details
	 * - 解决场景：每个插件 DLL 各自持有一份 `pcx::api` 单例，`pluginit` 时被写入全局
	 *   `ObjectManager*`；宿主 `Terminate` 销毁 `ObjectManager` 后，若不通知插件，插件
	 *   里的那份 `api::_objectManager` 会变成悬挂指针，后续任何 `CreateObject`/`FindObject`
	 *   等都会 UAF。
	 * - 触发时机：`PluginManager` 在收到 `SystemShutdown` 时反向遍历已加载插件调用本方法。
	 * - 兼容性：若插件未导出 `pluginexit`（老版本），则 no-op 且不告警。
	 */
	void exitPluginAPI();

private:
	/** @brief 禁止默认构造。 */
	Plugin() = delete;

	/**
	 * @brief 使用已加载句柄构造插件对象。
	 * @param[in] handle 动态库句柄。
	 * @param[in] path 插件路径。
	 */
	Plugin(void* handle, const char* path);

	/**
	 * @brief 调用插件初始化入口。
	 * @return 初始化成功返回 true，否则返回 false。
	 */
	bool initPluginAPI();

private:
	void*       _handle;    // 动态库句柄
	PluginInfo* _info;      // 插件信息指针（由插件导出）
	std::string _path;      // 插件路径
	void*       _initFunc;  // 插件初始化函数指针（pluginit）	
};

PCX_END_NAMESPACE