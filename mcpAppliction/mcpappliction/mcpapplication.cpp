#include "mcpapplication.h"

namespace mcp {

/**
 * @brief 按配置文件启动 MCP 应用框架（加载插件、预创建对象等）。
 * @param configPath TOML 配置路径
 * @return 成功 true；失败 false（详见日志）
 */
bool McpApplication::start(const char* configPath)
{
	/** 如果 MCP 应用框架实例为空，则创建 MCP 应用框架实例 */
	if (_application == nullptr) {
		_application = new ApplicationMCP(configPath);
		/** 启动 MCP 应用框架 */
		return _application->start(ApplicationMCP::MAIN_APP) == MCP_SUCCESS;
	}
	return true;
}

/**
 * @brief 停止 MCP 应用框架
 * @return 成功 true；失败 false（详见日志）
 */
bool McpApplication::stop()
{
	/** 如果 MCP 应用框架实例不为空，则停止 MCP 应用框架 */
	if (_application != nullptr)
		return _application->stop();
	return true;
}

/**
 * @brief 获取加载失败的插件路径列表和创建失败的对象（componentID/objectName）
 * @param[out] plugins 加载失败的插件路径列表
 * @param[out] components 创建失败的对象（componentID/objectName）
 */
void McpApplication::getLoadFailedPluginsInfo(std::vector<std::string>& plugins, std::vector<std::string>& components)
{
	/** 清空加载失败的插件路径列表 */
	plugins.clear();
	/** 清空创建失败的对象（componentID/objectName）列表 */
	components.clear();
	/** 如果 MCP 应用框架实例不为空，则获取加载失败的插件路径列表和创建失败的对象（componentID/objectName） */
	if (_application != nullptr)
		_application->getLoadFailedPluginsInfo(plugins, components);
}

}
