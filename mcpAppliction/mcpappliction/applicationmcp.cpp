#include "applicationmcp.h"
#include "configtoml.h"
#include <cstdio>

namespace mcp {

/**
 * @brief 检查对象管理器是否就绪
 * @return 就绪 true；未就绪 false
 */
static bool objectManagerReady()
{
	return mcp::api::FindObject(MCP_DEFAULT_OBJECTMANAGER) != nullptr;
}

/**
 * @brief 构造函数
 * @param configPath TOML 配置路径
 */
ApplicationMCP::ApplicationMCP(const char* configPath)
{
	createApplication(configPath);
}

/**
 * @brief 构造函数
 * @param config 配置对象
 */
ApplicationMCP::ApplicationMCP(ConfigBase* cfg)
{
	createApplication(cfg);
}

/**
 * @brief 创建应用
 * @param configPath TOML 配置路径
 */
void ApplicationMCP::createApplication(const char* configPath)
{
	_config = new ConfigToml(configPath);
}

/**
 * @brief 创建应用
 * @param config 配置对象
 */
void ApplicationMCP::createApplication(ConfigBase* cfg)
{
	if (cfg == nullptr)
		_config = new ConfigToml("./config.toml");
	else
		_config = cfg;
}

/**
 * @brief 析构函数
 */
ApplicationMCP::~ApplicationMCP()
{
	delete _config;
	_config = nullptr;
}

/**
 * @brief 启动应用
 * @param m 启动模式
 * @return 成功 MCP_SUCCESS；失败错误码
 */
ErrorCode ApplicationMCP::start(StartMode m)
{
	/** 加载配置 */
	if (!_config->load())
		return APP_LOAD_CONFIG_FAILED;

	/** 设置启动模式 */
	_mode = m;
	/** 主应用模式或组件模式下 */
	if (m == MAIN_APP || m == COMPONENT_APP) {

		/** 主应用模式 */
		if (m == MAIN_APP) {
			/** 检查对象管理器是否就绪 */
			if (objectManagerReady())
				return APP_API_CHECK_INIT_ERR;
			/** 初始化 MCP，失败返回 APP_API_INIT_FAILED；成功继续 */
			if (mcp::api::Initialize() == MCP_FAILED)
				return APP_API_INIT_FAILED;
		}

		/** 设置默认日志级别 */
		ILoggerManager* loggerManager = (ILoggerManager*)mcp::api::FindObject(MCP_DEFAULT_LOGGERMANAGER);
		if (loggerManager)
			loggerManager->setDefaultLoggerLevel(_config->getDefaultLogLevel());

		/** 清空加载失败的插件路径列表 */
		_vLoadFailedPlugins.clear();
		/** 获取插件管理器 */
		IPluginManager* pluginManager = (IPluginManager*)mcp::api::FindObject(MCP_DEFAULT_PLUGINMANAGER);
		if (pluginManager) {
			/** 遍历配置中的插件 */
			for (const auto& plg : _config->getPlugins()) {
				/** 如果插件路径不为空且加载失败，则添加到加载失败的插件路径列表 */
				if (!plg.empty() && pluginManager->loadPlugin(plg.c_str()) == nullptr)
					_vLoadFailedPlugins.push_back(plg);
			}
		}

		/** 清空创建失败的对象（componentID/objectName）列表 */
		_vCreateFailedComponents.clear();
		/** 遍历配置中的对象 */
		for (const auto& obj : _config->getObjects()) {
			/** 如果对象的初始化参数不为空  */
			if (obj._initParm.length() > 0) {
				/** 获取消息中心 */
				IMessageCenter* pMsgCenter = (IMessageCenter*)mcp::api::FindObject(MCP_DEFAULT_MESSAGECENTER);
				if (!pMsgCenter)
					return MCP_FAILED;
				/** 分配系统对象初始化消息 */
				IMessage* msg = pMsgCenter->allocMessage(
					SystemObjectInit,
					_config->getApplictionName(),
					static_cast<uint32_t>(obj._initParm.size() + 1),
					(void*)obj._initParm.c_str());
				/** 创建对象，失败返回错误码 */
				ErrorCode errCode = MCP_SUCCESS;
				mcp::api::CreateNamedObject(obj._componentID.c_str(), obj._objectName.c_str(), msg, &errCode);
				pMsgCenter->freeMessage(msg);
				/** 如果创建失败，则添加到创建失败的对象（componentID/objectName）列表 */
				if (errCode != MCP_SUCCESS) {
					_vCreateFailedComponents.push_back(obj._componentID + "/" + obj._objectName);
					return errCode;
				}
			}
			/** 如果对象的初始化参数为空 */
			else {
				/** 创建对象，失败返回空指针 */
				void* pObj = mcp::api::CreateNamedObject(obj._componentID.c_str(), obj._objectName.c_str());
				if (pObj == nullptr)
					_vCreateFailedComponents.push_back(obj._componentID + "/" + obj._objectName);
			}
		}
	} 
	else {
		/** 子应用模式下 */
		/** 检查对象管理器是否就绪 */	
		if (!objectManagerReady())
			return APP_API_INIT_FAILED;
	}

	/** 主应用模式或组件模式下，广播系统准备和运行消息 */
	if (_mode == MAIN_APP || m == COMPONENT_APP) {
		/** 获取消息中心 */
		IMessageCenter* pMsgCenter = (IMessageCenter*)mcp::api::FindObject(MCP_DEFAULT_MESSAGECENTER);
		if (pMsgCenter) {
			/** 分配系统准备消息 */
			IMessage* pMsg = pMsgCenter->allocMessage(SystemReady, nullptr, 0, nullptr, SystemMessage);
			pMsgCenter->broadcastLocalMessage(pMsg);
			/** 分配系统运行消息 */
			IMessage* pMsgRun = pMsgCenter->allocMessage(SystemRun, nullptr, 0, nullptr, SystemMessage);
			pMsgCenter->broadcastLocalMessage(pMsgRun);
		}
	}
	return MCP_SUCCESS;
}

/**
 * @brief 停止应用
 * @return 成功 true；失败 false
 */
bool ApplicationMCP::stop()
{
	/** 获取对象管理器 */
	IObjectManager* objManager = (IObjectManager*)mcp::api::FindObject(MCP_DEFAULT_OBJECTMANAGER);
	if (objManager) {
		/** 查找日志写入器 */
		const char* key[] = { "interface" };
		const char* value[] = { "mcp.ILoggerWrite" };
		/** 查找日志写入器数组 */
		IObjectArray* objArray = objManager->findObjectByInfo(key, value, 1);
		if (objArray) {
			/** 遍历日志写入器数组 */
			for (unsigned int i = 0; i < objArray->GetObjectCount(); i++) {
				/** 获取日志写入器 */
				ILoggerWrite* loggerWrite = (ILoggerWrite*)objArray->GetObject(i);
				/** 刷新日志写入器 */
				if (loggerWrite)
					loggerWrite->flush();
			}
			/** 释放日志写入器数组 */
			objManager->freeObjectArray(objArray);
		}
	}

	/** 主应用模式或组件模式下，广播系统停止和关闭消息 */
	if (_mode == MAIN_APP || _mode == COMPONENT_APP) {
		/** 获取消息中心 */
		IMessageCenter* pMsgCenter = (IMessageCenter*)mcp::api::FindObject(MCP_DEFAULT_MESSAGECENTER);
		if (pMsgCenter) {
			/** 分配系统停止消息 */
			IMessage* pMsg = pMsgCenter->allocMessage(SystemStop, nullptr, 0, nullptr, SystemMessage);
			pMsgCenter->broadcastLocalMessage(pMsg, ReverseOrder);
			/** 分配系统关闭消息 */
			IMessage* pMsgShutdown = pMsgCenter->allocMessage(SystemShutdown, nullptr, 0, nullptr, SystemMessage);
			pMsgCenter->broadcastLocalMessage(pMsgShutdown, ReverseOrder);
		}
	}
	return true;
}

/**
 * @brief 获取加载失败的插件路径列表和创建失败的对象（componentID/objectName）
 * @param[out] plugins 加载失败的插件路径列表
 * @param[out] components 创建失败的对象（componentID/objectName）
 */
void ApplicationMCP::getLoadFailedPluginsInfo(std::vector<std::string>& plugins, std::vector<std::string>& components)
{
	plugins = _vLoadFailedPlugins;
	components = _vCreateFailedComponents;
}

}
