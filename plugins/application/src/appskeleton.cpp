#include "appskeleton.h"
#include "configtoml.h"
#include <cstdio>

namespace pch {

/**
 * @brief 检查对象管理器是否就绪
 * @return 就绪返回 true；未就绪返回 false
 */
static bool objectManagerReady()
{
	return pch::api::FindObject(PCH_DEFAULT_OBJECTMANAGER) != nullptr;
}

/**
 * @brief 构造函数
 * @param configPath TOML 配置文件路径
 */
AppSkeleton::AppSkeleton(const char* configPath)
{
	createApplication(configPath);
}

/**
 * @brief 构造函数
 * @param config 配置对象
 */
AppSkeleton::AppSkeleton(ConfigBase* cfg)
{
	createApplication(cfg);
}

/**
 * @brief 创建应用
 * @param configPath TOML 配置文件路径
 */
void AppSkeleton::createApplication(const char* configPath)
{
	_config = new ConfigToml(configPath);
}

/**
 * @brief 创建应用
 * @param config 配置对象
 */
void AppSkeleton::createApplication(ConfigBase* cfg)
{
	if (cfg == nullptr)
		_config = new ConfigToml("./config.toml");
	else
		_config = cfg;
}

/**
 * @brief 析构函数
 */
AppSkeleton::~AppSkeleton()
{
	delete _config;
	_config = nullptr;
}

/**
 * @brief 启动应用
 * @param m 启动模式
 * @return 成功返回 PCH_SUCCESS；失败返回错误码
 */
ErrorCode AppSkeleton::start(StartMode m)
{
	// 加载配置
	if (!_config->load())
		return APP_LOAD_CONFIG_FAILED;

	// 设置启动模式
	_mode = m;
	// 主应用模式或组件模式
	if (m == MAIN_APP || m == COMPONENT_APP) {

		// 主应用模式
		if (m == MAIN_APP) {
			// 检查对象管理器是否就绪
			if (objectManagerReady())
				return APP_API_CHECK_INIT_ERR;
			// 初始化 PCH，失败返回 APP_API_INIT_FAILED；成功继续
			if (pch::api::Initialize() == PCH_FAILED)
				return APP_API_INIT_FAILED;
		}

		// 设置默认日志级别
		ILoggerManager* loggerManager = (ILoggerManager*)pch::api::FindObject(PCH_DEFAULT_LOGGERMANAGER);
		if (loggerManager)
			loggerManager->setDefaultLoggerLevel(_config->getDefaultLogLevel());

		// 清空加载失败的插件路径列表
		_vLoadFailedPlugins.clear();
		// 获取插件管理器
		IPluginManager* pluginManager = (IPluginManager*)pch::api::FindObject(PCH_DEFAULT_PLUGINMANAGER);
		if (pluginManager) {
			// 按配置数组顺序加载插件；单个失败仅记录路径，不中断后续插件（适用于部分可用部署）
			// 遍历配置中的插件
			for (const auto& plg : _config->getPlugins()) {
				// 如果插件路径不为空且加载失败，加入加载失败的插件路径列表
				if (!plg.empty() && pluginManager->loadPlugin(plg.c_str()) == nullptr)
					_vLoadFailedPlugins.push_back(plg);
			}
		}

		// 清空创建失败的对象（componentID/objectName）列表
		_vCreateFailedComponents.clear();
		// 预创建对象：可选的 SystemObjectInit 消息；失败写入 _vCreateFailedComponents 供宿主查询
		// 遍历配置中的对象
		for (const auto& obj : _config->getObjects()) {
			// 如果对象初始化参数不为空
			if (obj._initParm.length() > 0) {
				// 获取消息中心
				IMessageCenter* pMsgCenter = (IMessageCenter*)pch::api::FindObject(PCH_DEFAULT_MESSAGECENTER);
				if (!pMsgCenter)
					return PCH_FAILED;
				// 分配系统对象初始化消息
				IMessage* msg = pMsgCenter->allocMessage(
					SystemObjectInit,
					_config->getApplictionName(),
					static_cast<uint32_t>(obj._initParm.size() + 1),
					(void*)obj._initParm.c_str());
				// 创建对象，失败返回错误码
				ErrorCode errCode = PCH_SUCCESS;
				pch::api::CreateNamedObject(obj._componentID.c_str(), obj._objectName.c_str(), msg, &errCode);
				pMsgCenter->freeMessage(msg);
				// 如果创建失败，加入创建失败的对象列表
				if (errCode != PCH_SUCCESS) {
					_vCreateFailedComponents.push_back(obj._componentID + "/" + obj._objectName);
					return errCode;
				}
			}
			// 如果对象初始化参数为空
			else {
				// 创建对象，失败返回 nullptr
				void* pObj = pch::api::CreateNamedObject(obj._componentID.c_str(), obj._objectName.c_str());
				if (pObj == nullptr)
					_vCreateFailedComponents.push_back(obj._componentID + "/" + obj._objectName);
			}
		}

	}  // 主应用模式或组件模式结束
	else {
		// 子应用模式
		// 检查对象管理器是否就绪
		if (!objectManagerReady())
			return APP_API_INIT_FAILED;
	}

	// 主应用模式或组件模式，广播系统就绪和运行消息
	if (_mode == MAIN_APP || m == COMPONENT_APP) {
		// 获取消息中心
		IMessageCenter* pMsgCenter = (IMessageCenter*)pch::api::FindObject(PCH_DEFAULT_MESSAGECENTER);
		if (pMsgCenter) {
			// 分配系统就绪消息
			IMessage* pMsg = pMsgCenter->allocMessage(SystemReady, nullptr, 0, nullptr, SystemMessage);
			pMsgCenter->broadcastLocalMessage(pMsg);
			// 分配系统运行消息
			IMessage* pMsgRun = pMsgCenter->allocMessage(SystemRun, nullptr, 0, nullptr, SystemMessage);
			pMsgCenter->broadcastLocalMessage(pMsgRun);
		}
	}
	return PCH_SUCCESS;
}

/**
 * @brief 停止应用
 * @return 成功返回 true；失败返回 false
 */
bool AppSkeleton::stop()
{
	// 主应用模式或组件模式，广播系统停止和关闭消息
	if (_mode == MAIN_APP || _mode == COMPONENT_APP) {
		// 获取消息中心
		IMessageCenter* pMsgCenter = (IMessageCenter*)pch::api::FindObject(PCH_DEFAULT_MESSAGECENTER);
		if (pMsgCenter) {
			// 分配系统停止消息
			IMessage* pMsg = pMsgCenter->allocMessage(SystemStop, nullptr, 0, nullptr, SystemMessage);
			pMsgCenter->broadcastLocalMessage(pMsg, ReverseOrder);
			// 分配系统关闭消息
			IMessage* pMsgShutdown = pMsgCenter->allocMessage(SystemShutdown, nullptr, 0, nullptr, SystemMessage);
			pMsgCenter->broadcastLocalMessage(pMsgShutdown, ReverseOrder);
		}
	}
	return true;
}

/**
 * @brief 获取加载失败的插件路径列表和创建失败的对象列表（componentID/objectName）
 * @param[out] plugins 加载失败的插件路径列表
 * @param[out] components 创建失败的对象列表（componentID/objectName）
 */
void AppSkeleton::getLoadFailedPluginsInfo(std::vector<std::string>& plugins, std::vector<std::string>& components)
{
	plugins = _vLoadFailedPlugins;
	components = _vCreateFailedComponents;
}

}
