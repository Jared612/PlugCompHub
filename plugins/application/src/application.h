/**
 * @file Application.h
 * @brief PCH 应用框架：加载插件、预创建对象等
 * @details 实现 IApplication 接口，提供 PCH 应用启动/停止及失败插件/组件查询功能
 */
#pragma once

#include "application/iapplication.h"
#include "appskeleton.h"

namespace pch {

/**
 * @brief PCH 应用框架：加载插件、预创建对象等
 * @details 实现 IApplication 接口，提供 PCH 应用启动/停止及失败插件/组件查询功能
 */
class APP_API Application : public IApplication
{
public:
	/**
	 * @brief 构造函数
	 */
	Application() = default;

	/**
	 * @brief 通过配置文件启动 PCH 应用框架（加载插件、预创建对象等）
	 * @param configPath TOML 配置文件路径
	 * @return 成功返回 true；失败返回 false（详见日志）
	 */
	bool start(const char* configPath) override;

	/**
	 * @brief 停止应用并发送系统停止/关闭消息
	 * @return 成功返回 true；失败返回 false（详见日志）
	 */
	bool stop() override;

	/**
	 * @brief 获取加载失败的插件路径列表和创建失败的对象（componentID/objectName）
	 * @param[out] plugins 加载失败的插件路径列表
	 * @param[out] components 创建失败的对象列表（componentID/objectName）
	 */
	void getLoadFailedPluginsInfo(std::vector<std::string>& plugins, std::vector<std::string>& components) override;
		
private:
	AppSkeleton* _application = nullptr; // PCH 应用框架实例
};

}
