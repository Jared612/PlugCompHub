/**
 * @file ipcxapplication.h
 * @brief PCX 应用层封装：按配置文件驱动加载插件与预创建对象，并统一启停。
 * @details 典型实现解析 TOML，调用 `PluginManager` / `ObjectManager` 等；失败明细通过 `getLoadFailedPluginsInfo` 回溯。
 */
#pragma once
#include <string>
#include <vector>

#if defined(_WIN32)
#  if defined(PCX_APPLICATION_EXPORTS)
#    define PCX_APP_API __declspec(dllexport)
#  else
#    define PCX_APP_API __declspec(dllimport)
#  endif
#else
#  define PCX_APP_API
#endif

namespace pcx {

class PCX_APP_API IPcxApplication
{
public:
	/** @brief 虚析构，允许通过接口指针释放实现类。 */
	virtual ~IPcxApplication() = default;

	/**
	 * @brief 按配置文件启动：初始化 PCX（若尚未）、加载插件、按配置创建对象等。
	 * @param configPath TOML 配置文件路径（UTF-8）。
	 * @return 成功 true；任一关键步骤失败返回 false，并应写日志。
	 * @note 可重复调用行为以实现为准；通常仅允许单次成功 `start`。
	 */
	virtual bool start(const char* configPath) = 0;

	/**
	 * @brief 逆序停止：发系统消息、卸载插件或释放应用持有资源。
	 * @return 成功 true；部分资源清理失败时仍可能返回 false。
	 */
	virtual bool stop() = 0;

	/**
	 * @brief 汇总启动阶段失败项，便于宿主展示或重试。
	 * @param[out] plugins 动态库路径或标识字符串列表（加载 `dlopen`/`LoadLibrary` 失败项）。
	 * @param[out] components 创建失败描述，建议格式含 `componentID` 与对象名，便于定位配置行。
	 * @note 调用前可清空两个容器；实现可选择追加或覆盖。
	 */
	virtual void getLoadFailedPluginsInfo(std::vector<std::string>& plugins,std::vector<std::string>& components) = 0;
		
};

}
