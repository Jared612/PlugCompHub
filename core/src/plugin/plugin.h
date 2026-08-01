/**
 * @file plugin.h
 * @brief 单个插件动态库包装：加载/卸载、解析导出符号和组件表
 * @details 实现在 plugin.cpp 中；接口为 IPlugin
 */
#pragma once
#include "interface.h"
#include "interface.h"
#include "componentinfo.h"
#include "coreexport.h"
#include "plugininfo.h"
#include <string>

PCH_BEGIN_NAMESPACE

/**
 * @class Plugin
 * @brief 动态插件包装对象，实现 IPlugin，负责加载/卸载插件、解析导出符号和组件表
 */
class Plugin: public IPlugin
{
public:
	/**
	 * @brief 按路径加载插件
	 * @param[in] path 插件动态库路径
	 * @return 成功返回 Plugin 对象指针，失败返回 nullptr
	 */
	static Plugin* load(const char* path);

	/**
	 * @brief 析构插件对象并释放动态库句柄
	 */
	virtual ~Plugin();

	/**
	 * @brief 获取插件名称
	 * @return 插件名称字符串
	 */
	const char* getName();

	/**
	 * @brief 获取插件版本
	 * @return 插件版本字符串
	 */
	const char* getVersion();

	/**
	 * @brief 获取插件描述
	 * @return 插件描述字符串
	 */
	const char* getDescription();

	/**
	 * @brief 获取插件文件路径
	 * @return 插件路径字符串
	 */
	const char* getPath();

	/**
	 * @brief 获取插件导出的组件信息表
	 * @return 以 nullptr 结尾的组件信息表，失败返回 nullptr
	 */
	ComponentInfo** getComponentInfos();

	/**
	 * @brief 解析动态库导出的符号
	 * @param[in] symbol 符号名
	 * @return 成功返回符号地址，失败返回 nullptr
	 */
	void* resolve(const char* symbol);

	/**
	 * @brief 调用插件导出的 pluginexit（如果存在），将插件侧的 pch::api::_objectManager 置零
	 * @details
	 * - 场景：每个插件 DLL 都有自己的一份 pch::api 单例；pluginit 将全局
	 *   ObjectManager* 写入其中；主机 Terminate 销毁 ObjectManager 后，如果未通知插件，
	 *   插件侧 api::_objectManager 将成为悬空指针，后续 CreateObject/FindObject 等将产生 UAF。
	 * - 触发时机：PluginManager 在 SystemShutdown 时逆序遍历已加载插件并调用此方法。
	 * - 兼容性：如果插件未导出 pluginexit（旧版本），无操作且不产生警告。
	 */
	void exitPluginAPI();

private:
	/** @brief 禁用默认构造 */
	Plugin() = delete;

	/**
	 * @brief 使用已加载的句柄构造插件对象
	 * @param[in] handle 动态库句柄
	 * @param[in] path 插件路径
	 */
	Plugin(void* handle, const char* path);

	/**
	 * @brief 调用插件初始化入口
	 * @return 成功返回 true，失败返回 false
	 */
	bool initPluginAPI();

private:
	void*       _handle;    // 动态库句柄
	PluginInfo* _info;      // 插件信息指针（由插件导出）
	std::string _path;      // 插件路径
	void*       _initFunc;  // 插件初始化函数指针（pluginit）
};

PCH_END_NAMESPACE
