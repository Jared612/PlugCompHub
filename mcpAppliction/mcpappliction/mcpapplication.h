/**
 * @file mcpapplication.h
 * @brief MCP 应用框架：加载插件、预创建对象等。
 * @details 实现 IMcpApplication 接口，提供 MCP 应用的启停和失败插件/组件查询功能。
 */
#pragma once

#include "../include/imcpapplication.h"
#include "applicationmcp.h"

namespace mcp {

/**
 * @brief MCP 应用框架：加载插件、预创建对象等。
 * @details 实现 IMcpApplication 接口，提供 MCP 应用的启停和失败插件/组件查询功能。
 */
class MCP_APP_API McpApplication : public IMcpApplication
{
public:
	/**
	 * @brief 构造函数
	 */
	McpApplication() = default;

	/**
	 * @brief 按配置文件启动 MCP 应用框架（加载插件、预创建对象等）。
	 * @param configPath TOML 配置路径
	 * @return 成功 true；失败 false（详见日志）
	 */
	bool start(const char* configPath) override;

	/**
	 * @brief 停止应用并发出系统停止/关闭消息。
	 * @return 成功 true；失败 false（详见日志）
	 */
	bool stop() override;

	/**
	 * @brief 获取加载失败的插件路径列表和创建失败的对象（componentID/objectName）
	 * @param[out] plugins 加载失败的插件路径列表
	 * @param[out] components 创建失败的对象（componentID/objectName）
	 */
	void getLoadFailedPluginsInfo(std::vector<std::string>& plugins, std::vector<std::string>& components) override;
		
private:
	ApplicationMCP* _application = nullptr; // MCP 应用框架实例
};

}
