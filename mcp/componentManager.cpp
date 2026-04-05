#include <string.h>
#include "componentManager.h"
#include "error.h"
#include "loggerManager.h"

MCP_REGISTER_COMPONENT(mcp::ComponentManager, MCP_COMPONENT_MANAGER_ID)

MCP_BEGIN_NAMESPACE

/*
 * @brief 组建管理器默认析构函数
 * @param:  none
 * @retval: none
 */
ComponentManager::~ComponentManager()
{
	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 释放所有组件对象
	for (auto it = _components.begin() ; it != _components.end() ; it++){
		delete it->second;
	}

	// 释放所有延迟删除的组件对象
	for (auto it = _deleteComponents.begin(); it != _deleteComponents.end(); it++){
		delete *it;
	}

	// 清空延迟删除的组件列表，清空组件列表
	_deleteComponents.clear();
	_components.clear();
}

/**
 * @brief 注册已构造好的组件对象。
 * @param compInfo 组件静态信息（用于取 componentID）
 * @param comp 组件封装对象
 * @return ErrorCode 注册结果
 */
ErrorCode ComponentManager::registerComponent(ComponentInfo* compInfo, Component* comp)
{
	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 如果组件不存在，则注册组件
	if (_components.find(compInfo->componentID) == _components.end()) {
		_components[compInfo->componentID] = comp;
	} 
	else {
		// 如果组件已存在，则返回错误码
		WriteLog(McpLogLevel::Fatal, "Component %s exists!", compInfo->componentID);
		return MCP_COMPONNET_ADYEXIST;
	}
	// 注册成功，返回成功码
	return MCP_SUCCESS;
}

/**
 * @brief 组件注册
 * @param compInfo 组件信息指针
 * @retval	MCP_SUCCESS 注册成功
 *			MCP_COMPONENT_NULLPTR：参数为空
 *			MCP_COMPONENT_INVALID：参数无效
 */
ErrorCode ComponentManager::registerComponent(ComponentInfo* compInfo)
{
	// 检查组件信息是否为空，如果为空，则返回错误码
	if (compInfo == nullptr)
		return MCP_COMPONENT_NULLPTR;

	// 检查组件信息是否有效，如果无效，则返回错误码
	if (compInfo->creator == nullptr || compInfo->deletor == nullptr){
		WriteLog(McpLogLevel::Error, "Component[%s] no creator or deletor", compInfo->componentID);
		return MCP_COMPONENT_INVALID;
	}

	// 注册组件
	{
		// 加锁，防止多线程同时访问
		std::lock_guard<std::mutex> lk(_mutex);
		// 如果组件不存在，则注册组件
		if (_components.find(compInfo->componentID) == _components.end()) {
			_components[compInfo->componentID] = new Component(compInfo);
		} 
		else {
			// 如果组件已存在，则返回错误码
			WriteLog(McpLogLevel::Information, "Component %s exists!", compInfo->componentID);
			delete compInfo;
			return MCP_COMPONNET_ADYEXIST;
		}
	}

	WriteLog(McpLogLevel::Debug, "Register component[%s] succeed!", compInfo->componentID);
	return MCP_SUCCESS;
}

/**
* @brief 批量注册组件
* @param cmptable 组件信息数组
* @return 注册结果错误码
*/
ErrorCode ComponentManager::registerComponents(ComponentInfo** cmptable)
{
	// 检查组件信息数组是否为空，如果为空，则返回错误码
	if (cmptable == nullptr)
		return MCP_COMPONENT_NULLPTR;

	// 遍历组件信息数组，注册组件
	for (int i = 0; cmptable[i] != nullptr; i++) {
		ErrorCode res = registerComponent(cmptable[i]);
		if (res != MCP_SUCCESS) {
			return res;
		}
	}

	// 注册成功，返回成功
	return MCP_SUCCESS;
}

/**
 * @brief 按组件信息注销组件
 * @param compInfo 组件信息指针
 * @return 注销结果错误码
 */
ErrorCode ComponentManager::unregisterComponent(ComponentInfo* compInfo)
{
	// 检查组件信息是否为空，如果为空，则返回错误码
	if (compInfo == nullptr)
		return MCP_NULLPTR;

	// 注销组件
	return unregisterComponent(compInfo->componentID);
}

/**
 * @brief 按组件 ID 注销组件
 * @param componentID 组件唯一标识
 * @return 注销结果错误码
 */
ErrorCode ComponentManager::unregisterComponent(const char* componentID)
{
	// 检查组件唯一标识是否为空，如果为空，则返回错误码
	if (componentID == nullptr || strcmp(componentID, "") == 0) {
		WriteLog(McpLogLevel::Debug, "ComponentID is nullptr or empty");
		return MCP_PARAM_NULLPTR;
	}

	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 查找组件
	auto it = _components.find(componentID);
	if (it != _components.end()) {
		// 如果组件不为空，则放入待删除队列	
		if (it->second != nullptr) {
			// 放入待删除队列
			_deleteComponents.push_back(it->second);
        }
		// 删除组件
		_components.erase(it);
		return MCP_SUCCESS;
	}
	// 如果组件不存在，则返回错误码
	else {
		return MCP_COMPONENT_NOTFOUND;
	}
}

/**
* @brief 批量注销组件
* @param cmptable 组件信息数组
* @return 注销结果错误码
*/
ErrorCode ComponentManager::unregisterComponents(ComponentInfo** cmptable)
{
	// 检查组件信息数组是否为空，如果为空，则返回错误码
	if (cmptable == nullptr)
		return MCP_NULLPTR;

	// 遍历组件信息数组，注销组件
	for (int i = 0; cmptable[i] != nullptr; i++) {
		ErrorCode res = unregisterComponent(cmptable[i]);
		if (res != MCP_SUCCESS) {
			return res;
		}
	}

	// 注销成功，返回成功码	
	return MCP_SUCCESS;
}


/**
 * @brief 按组件 ID 获取组件信息
 * @param componentID 组件唯一标识
 * @return 组件信息对象指针，如果组件不存在，则返回 nullptr
 */
ComponentInfo* ComponentManager::getComponentInf(const char* componentID)
{
	// 检查组件唯一标识是否为空，如果为空，则返回 nullptr
	if (componentID == nullptr || strcmp(componentID, "") == 0) {
		WriteLog(McpLogLevel::Debug, "ComponentID is nullptr");
		return nullptr;
	}

	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 查找组件
	auto it = _components.find(componentID);
	
	// 如果组件不存在，则返回 nullptr
	if (it == _components.end()) {
		WriteLog(McpLogLevel::Warning, "Component[%s] not found", componentID);
		return nullptr;
	}

	// 返回组件信息
	return it->second->getComponentInfo();
}

/**
 * @brief 按组件 ID 查找组件封装对象。
 * @param componentID 组件唯一标识
 * @return Component* 命中返回组件对象，否则返回 nullptr
 */
Component* ComponentManager::findComp(const char* componentID)
{
	// 检查组件唯一标识是否为空，如果为空，则返回 nullptr
	if (componentID == nullptr || strcmp(componentID, "") == 0) {
		WriteLog(McpLogLevel::Debug, "ComponentID is nullptr");
		return nullptr;
	}

	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 查找组件
	auto it = _components.find(componentID);
	// 如果组件不存在，则返回 nullptr
	if (it == _components.end()) {
		WriteLog(McpLogLevel::Warning, "Component[%s] not found", componentID);
		return nullptr;
	}

	// 返回组件对象
	return it->second;
}

/**
 * @brief 获取当前已加载组件 ID 列表
 * @return 已加载组件 ID 列表
 */
std::list<std::string> ComponentManager::getLoadedComponents()
{
	std::list<std::string> componentNames;
	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 遍历组件映射表，获取组件 ID 列表
	for (auto it = _components.begin(); it != _components.end(); it++) {
		componentNames.push_back(it->first);
	}

	// 返回组件 ID 列表
	return componentNames;
}

MCP_END_NAMESPACE