/**
 * @file configtoml.h
 * @brief 用 cpptoml 读取 `.toml`，填充 `ConfigBase`。
 */
#pragma once
#include "configbase.h"
#include "cpptoml.h"

namespace mcp {

/**
 * @brief ConfigToml 类，继承自 ConfigBase 类，用于读取 `.toml` 配置文件。
 */
class ConfigToml : public ConfigBase
{
public:
	/**
	 * @brief 构造函数
	 * @param configPath 配置文件路径
	 */
	explicit ConfigToml(const char* configPath);

	/**
	 * @brief 加载配置
	 * @param pluginsKey 插件键，默认 "plugins"
	 * @return 是否成功
	 */
	virtual bool load(const char* pluginsKey = "plugins") override;

protected:
	/** cpptoml 表 */
	std::shared_ptr<cpptoml::table> _root = nullptr;
};

}
