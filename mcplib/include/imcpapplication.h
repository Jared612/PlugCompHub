/**
 * @file imcpapplication.h
 * @brief MCP 应用接口：启停、失败插件/组件查询。
 * @details 实现 MCP 应用的启停和失败插件/组件查询功能。
 */
#pragma once
#include <string>
#include <vector>

#if defined(_WIN32)
#  if defined(MCP_APPLICATION_EXPORTS)
#    define MCP_APP_API __declspec(dllexport)
#  else
#    define MCP_APP_API __declspec(dllimport)
#  endif
#else
#  define MCP_APP_API
#endif

namespace mcp {

class MCP_APP_API IMcpApplication
{
public:
	/**
	 * @brief 析构函数
	 */
	virtual ~IMcpApplication() = default;

	/**
	 * @brief 按配置文件启动 MCP 应用（加载插件、预创建对象等）。
	 * @param configPath TOML 配置路径
	 * @return 成功 true；失败 false（详见日志）
	 */
	virtual bool start(const char* configPath) = 0;

	/**
	 * @brief 停止应用并发出系统停止/关闭消息。
	 */
	virtual bool stop() = 0;

	/**
	 * @brief 获取加载失败的插件路径列表和创建失败的对象（componentID/objectName）
	 * @param[out] plugins 加载失败的插件路径列表
	 * @param[out] components 创建失败的对象（componentID/objectName）
	 */
	virtual void getLoadFailedPluginsInfo(std::vector<std::string>& plugins,std::vector<std::string>& components) = 0;
		
};

}
