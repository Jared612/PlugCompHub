/**
 * @file applicationmcp.h
 * @brief 基于 mcp::api 的应用骨架：配置、插件加载、系统消息与生命周期。
 */
#pragma once
#include "configbase.h"
#include "internal.h"
#include "mcp.h"

namespace mcp {


constexpr int APP_SUCC               = 0;       //`start()` 成功或与 MCP_SUCCESS(0) 一致时的应用层返回值。
constexpr int APP_LOAD_CONFIG_FAILED = -10000;  //配置 `load()` 失败（如 TOML 解析错误）
constexpr int APP_API_INIT_FAILED    = -10000;  //`mcp::api::Initialize` 失败或对象管理器未就绪等初始化错误。
constexpr int APP_API_CHECK_INIT_ERR = -10001;  //主应用模式下对象管理器已存在，与「尚未 Initialize」的约定冲突。

/**
 * @brief 基于 mcp::api 的应用框架：配置、插件加载、系统消息与生命周期。
 * @details 是套在 mcp::api 外面的「应用启动流程」：用一份 ConfigBase/ConfigToml 配置，按约定把 MCP 跑起来并拉起业务。
 */
class ApplicationMCP
{
protected:
	/** @brief 消息处理函数类型 */
	typedef const mcp::IMessage* (ApplicationMCP::*FuncMessageHandle)(const mcp::IMessage* msg);

public:
	/**
	 * @brief 构造函数
	 * @param configPath TOML 配置路径
	 */
	explicit ApplicationMCP(const char* configPath);

	/**
	 * @brief 构造函数
	 * @param config 配置对象
	 */
	explicit ApplicationMCP(ConfigBase* config);

	/**
	 * @brief 析构函数
	 */
	virtual ~ApplicationMCP();

	/** @brief 启动模式 */
	enum StartMode
	{
		MAIN_APP = 0,			//主应用模式
		CHILD_APP = 1,			//子应用模式
		COMPONENT_APP = 2,		//组件模式
	};

	/**
	 * @brief 启动应用
	 * @param m 启动模式
	 * @return 成功 MCP_SUCCESS；失败错误码
	 */
	virtual ErrorCode start(StartMode m = MAIN_APP);

	/**
	 * @brief 停止应用
	 * @return 成功 true；失败 false
	 */
	virtual bool stop();

	/**
	 * @brief 获取加载失败的插件路径列表和创建失败的对象（componentID/objectName）
	 * @param[out] plugins 加载失败的插件路径列表
	 * @param[out] components 创建失败的对象（componentID/objectName）
	 */
	void getLoadFailedPluginsInfo(std::vector<std::string>& plugins, std::vector<std::string>& components);

protected:
	ConfigBase*              _config = nullptr;         //配置对象
	StartMode                _mode = MAIN_APP;          //启动模式	
	std::vector<std::string> _vLoadFailedPlugins;       //加载失败的插件路径列表
	std::vector<std::string> _vCreateFailedComponents;  //创建失败的对象（componentID/objectName）
private:
	/**
	 * @brief 创建应用
	 * @param configPath TOML 配置路径
	 */
	void createApplication(const char* configPath);
	/**
	 * @brief 创建应用
	 * @param config 配置对象
	 */
	void createApplication(ConfigBase* cfg);
};

}
