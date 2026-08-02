#include "application.h"

namespace pch {

/**
 * @brief 通过配置文件启动 PCH 应用框架（加载插件、预创建对象等）
 * @param configPath TOML 配置文件路径
 * @return 成功返回 true；失败返回 false（详见日志）
 */
bool Application::start(const char* configPath)
{
	// 薄封装层：持有 AppSkeleton 生命周期；在 MAIN_APP 模式下内部初始化 PCH
	// 如果 PCH 应用框架实例为空，则创建一个
	if (_application == nullptr) {
		_application = new AppSkeleton(configPath);
		// 启动 PCH 应用框架
		return _application->start(AppSkeleton::MAIN_APP) == PCH_SUCCESS;
	}
	return true;
}

/**
 * @brief 停止 PCH 应用框架
 * @return 成功返回 true；失败返回 false（详见日志）
 */
bool Application::stop()
{
	// 如果 PCH 应用框架实例不为空，则停止它
	if (_application != nullptr)
		return _application->stop();
	return true;
}

/**
 * @brief 获取加载失败的插件路径列表和创建失败的对象列表（componentID/objectName）
 * @param[out] plugins 加载失败的插件路径列表
 * @param[out] components 创建失败的对象列表（componentID/objectName）
 */
void Application::getLoadFailedPluginsInfo(std::vector<std::string>& plugins, std::vector<std::string>& components)
{
	// 清空加载失败的插件路径列表
	plugins.clear();
	// 清空创建失败的对象（componentID/objectName）列表
	components.clear();
	// 如果 PCH 应用框架实例不为空，则获取加载失败的插件路径列表和创建失败的对象
	if (_application != nullptr)
		_application->getLoadFailedPluginsInfo(plugins, components);
}

}
