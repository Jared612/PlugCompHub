#include <string.h>
#include "componentManager.h"
#include "error.h"
#include "loggerManager.h"

PCH_REGISTER_COMPONENT(pch::ComponentManager, PCH_COMPONENT_MANAGER_ID)

PCH_BEGIN_NAMESPACE

// 组件管理器默认析构函数
ComponentManager::~ComponentManager()
{
	// 加锁防止并发访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 释放所有组件对象
	for (auto it = _components.begin() ; it != _components.end() ; it++){
		delete it->second;
	}

	// 释放所有延迟删除的组件对象
	for (auto it = _deleteComponents.begin(); it != _deleteComponents.end(); it++){
		delete *it;
	}

	// 清空延迟删除组件列表和组件列表
	_deleteComponents.clear();
	_components.clear();
}

/**
 * @brief 注册已构造好的组件包装对象
 * @param compInfo 组件静态信息（用于 componentID）；由 PCH_REGISTER_COMPONENT 生成，静态存储，管理器不管理其生命周期
 * @param comp     组件包装对象；成功注册后生命周期由本管理器管理；重复注册时本函数会调用 `delete comp` 以对齐所有权约定
 * @return ErrorCode 注册结果
 */
ErrorCode ComponentManager::registerComponent(ComponentInfo* compInfo, Component* comp)
{
	// 参数校验：compInfo 或 comp 为空时无法建立映射
	if (compInfo == nullptr || comp == nullptr) {
		WriteLog(LogLevel::Error, "registerComponent: compInfo or comp is nullptr");
		// 调用方仍然拥有 comp（如果非空）；此处不干预其生命周期
		return PCH_COMPONENT_NULLPTR;
	}
	if (compInfo->componentID == nullptr || compInfo->componentID[0] == '\0') {
		WriteLog(LogLevel::Error, "registerComponent: componentID is empty");
		return PCH_COMPONENT_INVALID;
	}

	// 加锁防止并发访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 如果组件不存在，则注册
	if (_components.find(compInfo->componentID) == _components.end()) {
		_components[compInfo->componentID] = comp;
		return PCH_SUCCESS;
	}

	// 重复注册：按约定接管 comp 的释放，避免调用方和本管理器双重释放
	WriteLog(LogLevel::Warning, "Component %s exists!", compInfo->componentID);
	delete comp;
	return PCH_COMPONNET_ADYEXIST;
}

/**
 * @brief 组件注册
 * @param compInfo 组件信息指针
 * @retval PCH_SUCCESS 注册成功
 *         PCH_COMPONENT_NULLPTR：参数为空
 *         PCH_COMPONENT_INVALID：参数无效
 */
ErrorCode ComponentManager::registerComponent(ComponentInfo* compInfo)
{
	// 检查组件信息是否为空
	if (compInfo == nullptr)
		return PCH_COMPONENT_NULLPTR;

	// 检查组件信息是否有效
	if (compInfo->creator == nullptr || compInfo->deletor == nullptr){
		WriteLog(LogLevel::Error, "Component[%s] no creator or deletor",
				 compInfo->componentID ? compInfo->componentID : "(null)");
		return PCH_COMPONENT_INVALID;
	}
	if (compInfo->componentID == nullptr || compInfo->componentID[0] == '\0') {
		WriteLog(LogLevel::Error, "Component has empty componentID");
		return PCH_COMPONENT_INVALID;
	}

	// 通过 nothrow 预先分配 Component 包装；失败时立即返回，不影响调用方的 compInfo
	Component* comp = new (std::nothrow) Component(compInfo);
	if (comp == nullptr) {
		WriteLog(LogLevel::Fatal, "Alloc Component for [%s] failed", compInfo->componentID);
		return PCH_OUTOFMEM;
	}

	// 注册组件
	{
		// 加锁防止并发访问
		std::lock_guard<std::mutex> lk(_mutex);
		if (_components.find(compInfo->componentID) == _components.end()) {
			_components[compInfo->componentID] = comp;
		}
		else {
			// 重复注册：仅释放本函数分配的包装对象；
			// ComponentInfo 是静态存储，不在本模块的所有权范围内
			WriteLog(LogLevel::Information, "Component %s exists!", compInfo->componentID);
			delete comp;
			return PCH_COMPONNET_ADYEXIST;
		}
	}

	WriteLog(LogLevel::Debug, "Register component[%s] succeed!", compInfo->componentID);
	return PCH_SUCCESS;
}

/**
* @brief 批量注册组件
* @param cmptable 组件信息数组
* @return 注册结果错误码
*/
ErrorCode ComponentManager::registerComponents(ComponentInfo** cmptable)
{
	// 检查组件信息数组是否为空
	if (cmptable == nullptr)
		return PCH_COMPONENT_NULLPTR;

	// 遍历组件信息数组，逐个注册；单个失败不影响其他，保留首个错误码用于诊断
	ErrorCode firstErr = PCH_SUCCESS;
	for (int i = 0; cmptable[i] != nullptr; i++) {
		ErrorCode res = registerComponent(cmptable[i]);
		if (res != PCH_SUCCESS && firstErr == PCH_SUCCESS) {
			firstErr = res;
		}
	}

	return firstErr;
}

/**
 * @brief 按组件信息注销组件
 * @param compInfo 组件信息指针
 * @return 注销结果错误码
 */
ErrorCode ComponentManager::unregisterComponent(ComponentInfo* compInfo)
{
	// 检查组件信息是否为空
	if (compInfo == nullptr)
		return PCH_NULLPTR;

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
	// 检查组件唯一标识是否为空
	if (componentID == nullptr || strcmp(componentID, "") == 0) {
		WriteLog(LogLevel::Debug, "ComponentID is nullptr or empty");
		return PCH_PARAM_NULLPTR;
	}

	// 加锁防止并发访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 查找组件
	auto it = _components.find(componentID);
	if (it != _components.end()) {
		// 如果组件不为空，放入延迟删除队列
		if (it->second != nullptr) {
			// 放入延迟删除队列
			_deleteComponents.push_back(it->second);
        }
		// 移除组件
		_components.erase(it);
		return PCH_SUCCESS;
	}
	// 如果组件不存在，返回错误码
	else {
		return PCH_COMPONENT_NOTFOUND;
	}
}

/**
* @brief 批量注销组件
* @param cmptable 组件信息数组
* @return 注销结果错误码
*/
ErrorCode ComponentManager::unregisterComponents(ComponentInfo** cmptable)
{
	// 检查组件信息数组是否为空
	if (cmptable == nullptr)
		return PCH_NULLPTR;

	// 单个失败不影响其他（与 registerComponents 对称），避免批量卸载时部分注销失败
	ErrorCode firstErr = PCH_SUCCESS;
	for (int i = 0; cmptable[i] != nullptr; i++) {
		ErrorCode res = unregisterComponent(cmptable[i]);
		if (res != PCH_SUCCESS && firstErr == PCH_SUCCESS) {
			firstErr = res;
		}
	}

	return firstErr;
}


/**
 * @brief 按组件 ID 获取组件信息
 * @param componentID 组件唯一标识
 * @return 组件信息对象指针，组件不存在返回 nullptr
 */
ComponentInfo* ComponentManager::getComponentInf(const char* componentID)
{
	// 检查组件唯一标识是否为空
	if (componentID == nullptr || strcmp(componentID, "") == 0) {
		WriteLog(LogLevel::Debug, "ComponentID is nullptr");
		return nullptr;
	}

	// 加锁防止并发访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 查找组件
	auto it = _components.find(componentID);
	
	// 如果组件不存在，返回 nullptr
	if (it == _components.end()) {
		WriteLog(LogLevel::Warning, "Component[%s] not found", componentID);
		return nullptr;
	}

	// 返回组件信息
	return it->second->getComponentInfo();
}

/**
 * @brief 按组件 ID 查找组件包装对象
 * @param componentID 组件唯一标识
 * @return 命中返回 Component*，否则返回 nullptr
 */
Component* ComponentManager::findComp(const char* componentID)
{
	// 检查组件唯一标识是否为空
	if (componentID == nullptr || strcmp(componentID, "") == 0) {
		WriteLog(LogLevel::Debug, "ComponentID is nullptr");
		return nullptr;
	}

	// 加锁防止并发访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 查找组件
	auto it = _components.find(componentID);
	// 如果组件不存在，返回 nullptr
	if (it == _components.end()) {
		WriteLog(LogLevel::Warning, "Component[%s] not found", componentID);
		return nullptr;
	}

	// 返回组件对象
	return it->second;
}

/**
 * @brief 将 compInfo 对应的包装对象从延迟删除队列中恢复到注册表
 * @details 当插件 unloadPlugin 回滚失败需要恢复时，应优先使用此函数重用现有 Component*；
 *          避免走 registerComponent(compInfo) 新建包装，而旧包装仍留在 _deleteComponents 中。
 */
ErrorCode ComponentManager::reregisterFromDeleteList(ComponentInfo* compInfo)
{
	if (compInfo == nullptr) {
		return PCH_COMPONENT_NULLPTR;
	}
	if (compInfo->componentID == nullptr || compInfo->componentID[0] == '\0') {
		return PCH_COMPONENT_INVALID;
	}

	std::lock_guard<std::mutex> lk(_mutex);

	// 在延迟删除队列中查找相同 componentID 的包装；相同的 ComponentInfo* 也算命中
	for (auto it = _deleteComponents.begin(); it != _deleteComponents.end(); ++it) {
		Component* comp = *it;
		if (comp == nullptr) {
			continue;
		}
		ComponentInfo* ci = comp->getComponentInfo();
		const bool sameById = ci != nullptr && ci->componentID != nullptr &&
			strcmp(ci->componentID, compInfo->componentID) == 0;
		if (!sameById && ci != compInfo) {
			continue;
		}

		// 如果同 ID 已在注册表中，保持现状，旧包装留在延迟队列中等析构时回收
		if (_components.find(compInfo->componentID) != _components.end()) {
			return PCH_COMPONNET_ADYEXIST;
		}

		_components[compInfo->componentID] = comp;
		_deleteComponents.erase(it);
		return PCH_SUCCESS;
	}

	return PCH_COMPONENT_NOTFOUND;
}

/**
 * @brief 获取当前已加载的组件 ID 列表
 * @return 已加载的组件 ID 列表
 */
std::list<std::string> ComponentManager::getLoadedComponents()
{
	std::list<std::string> componentNames;
	// 加锁防止并发访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 遍历组件映射表，获取组件 ID 列表
	for (auto it = _components.begin(); it != _components.end(); it++) {
		componentNames.push_back(it->first);
	}

	// 返回组件 ID 列表
	return componentNames;
}

PCH_END_NAMESPACE
