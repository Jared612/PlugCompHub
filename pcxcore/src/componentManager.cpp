#include <string.h>
#include "componentManager.h"
#include "error.h"
#include "loggerManager.h"

PCX_REGISTER_COMPONENT(pcx::ComponentManager, PCX_COMPONENT_MANAGER_ID)

PCX_BEGIN_NAMESPACE

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
 * @param compInfo 组件静态信息（用于取 componentID）；由 `PCX_REGISTER_COMPONENT` 生成，静态存储，管理器不接管其生命周期
 * @param comp    组件封装对象；注册成功后由本管理器负责释放；重复注册时本函数会 `delete comp` 以对齐所有权约定
 * @return ErrorCode 注册结果
 */
ErrorCode ComponentManager::registerComponent(ComponentInfo* compInfo, Component* comp)
{
	// 参数校验：compInfo / comp 任一为空都无法建立映射
	if (compInfo == nullptr || comp == nullptr) {
		WriteLog(PcxLogLevel::Error, "registerComponent: compInfo or comp is nullptr");
		// 调用方仍拥有 comp（若非空）的所有权；此处不动其生命周期
		return PCX_COMPONENT_NULLPTR;
	}
	if (compInfo->componentID == nullptr || compInfo->componentID[0] == '\0') {
		WriteLog(PcxLogLevel::Error, "registerComponent: componentID is empty");
		return PCX_COMPONENT_INVALID;
	}

	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 如果组件不存在，则注册组件
	if (_components.find(compInfo->componentID) == _components.end()) {
		_components[compInfo->componentID] = comp;
		return PCX_SUCCESS;
	}

	// 重复注册：按约定接管 comp 的释放，避免调用方与本管理器重复释放
	WriteLog(PcxLogLevel::Warning, "Component %s exists!", compInfo->componentID);
	delete comp;
	return PCX_COMPONNET_ADYEXIST;
}

/**
 * @brief 组件注册
 * @param compInfo 组件信息指针
 * @retval	PCX_SUCCESS 注册成功
 *			PCX_COMPONENT_NULLPTR：参数为空
 *			PCX_COMPONENT_INVALID：参数无效
 */
ErrorCode ComponentManager::registerComponent(ComponentInfo* compInfo)
{
	// 检查组件信息是否为空，如果为空，则返回错误码
	if (compInfo == nullptr)
		return PCX_COMPONENT_NULLPTR;

	// 检查组件信息是否有效，如果无效，则返回错误码
	if (compInfo->creator == nullptr || compInfo->deletor == nullptr){
		WriteLog(PcxLogLevel::Error, "Component[%s] no creator or deletor",
				 compInfo->componentID ? compInfo->componentID : "(null)");
		return PCX_COMPONENT_INVALID;
	}
	if (compInfo->componentID == nullptr || compInfo->componentID[0] == '\0') {
		WriteLog(PcxLogLevel::Error, "Component has empty componentID");
		return PCX_COMPONENT_INVALID;
	}

	// 提前以 nothrow 分配 Component 封装；失败直接返回，不影响调用方持有的 compInfo
	Component* comp = new (std::nothrow) Component(compInfo);
	if (comp == nullptr) {
		WriteLog(PcxLogLevel::Fatal, "Alloc Component for [%s] failed", compInfo->componentID);
		return PCX_OUTOFMEM;
	}

	// 注册组件
	{
		// 加锁，防止多线程同时访问
		std::lock_guard<std::mutex> lk(_mutex);
		if (_components.find(compInfo->componentID) == _components.end()) {
			_components[compInfo->componentID] = comp;
		}
		else {
			// 重复注册：仅释放本函数分配的封装对象；ComponentInfo 为静态存储，不在本模块所有权范围内
			WriteLog(PcxLogLevel::Information, "Component %s exists!", compInfo->componentID);
			delete comp;
			return PCX_COMPONNET_ADYEXIST;
		}
	}

	WriteLog(PcxLogLevel::Debug, "Register component[%s] succeed!", compInfo->componentID);
	return PCX_SUCCESS;
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
		return PCX_COMPONENT_NULLPTR;

	// 遍历组件信息数组，注册组件；单项失败不阻断其它组件，保留首个错误码以便诊断
	ErrorCode firstErr = PCX_SUCCESS;
	for (int i = 0; cmptable[i] != nullptr; i++) {
		ErrorCode res = registerComponent(cmptable[i]);
		if (res != PCX_SUCCESS && firstErr == PCX_SUCCESS) {
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
	// 检查组件信息是否为空，如果为空，则返回错误码
	if (compInfo == nullptr)
		return PCX_NULLPTR;

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
		WriteLog(PcxLogLevel::Debug, "ComponentID is nullptr or empty");
		return PCX_PARAM_NULLPTR;
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
		return PCX_SUCCESS;
	}
	// 如果组件不存在，则返回错误码
	else {
		return PCX_COMPONENT_NOTFOUND;
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
		return PCX_NULLPTR;

	// 单项失败不阻断其它组件（与 `registerComponents` 对称），避免批量卸载半途中止造成半注销
	ErrorCode firstErr = PCX_SUCCESS;
	for (int i = 0; cmptable[i] != nullptr; i++) {
		ErrorCode res = unregisterComponent(cmptable[i]);
		if (res != PCX_SUCCESS && firstErr == PCX_SUCCESS) {
			firstErr = res;
		}
	}

	return firstErr;
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
		WriteLog(PcxLogLevel::Debug, "ComponentID is nullptr");
		return nullptr;
	}

	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 查找组件
	auto it = _components.find(componentID);
	
	// 如果组件不存在，则返回 nullptr
	if (it == _components.end()) {
		WriteLog(PcxLogLevel::Warning, "Component[%s] not found", componentID);
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
		WriteLog(PcxLogLevel::Debug, "ComponentID is nullptr");
		return nullptr;
	}

	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 查找组件
	auto it = _components.find(componentID);
	// 如果组件不存在，则返回 nullptr
	if (it == _components.end()) {
		WriteLog(PcxLogLevel::Warning, "Component[%s] not found", componentID);
		return nullptr;
	}

	// 返回组件对象
	return it->second;
}

/**
 * @brief 从延迟删除队列里把 `compInfo` 对应的包装对象还原到注册表。
 * @details 当插件 `unloadPlugin` 判活失败需要回滚时，应优先走本函数复用旧 `Component*`，
 *          避免再走 `registerComponent(compInfo)` 产生新包装、把旧包装遗留在 `_deleteComponents`。
 */
ErrorCode ComponentManager::reregisterFromDeleteList(ComponentInfo* compInfo)
{
	if (compInfo == nullptr) {
		return PCX_COMPONENT_NULLPTR;
	}
	if (compInfo->componentID == nullptr || compInfo->componentID[0] == '\0') {
		return PCX_COMPONENT_INVALID;
	}

	std::lock_guard<std::mutex> lk(_mutex);

	// 在延迟删除队列中查找 componentID 相同的包装；相同 ComponentInfo* 也视为命中
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

		// 若注册表里已经存在同 ID 项，保留现状，旧包装仍留在延迟队列由析构回收
		if (_components.find(compInfo->componentID) != _components.end()) {
			return PCX_COMPONNET_ADYEXIST;
		}

		_components[compInfo->componentID] = comp;
		_deleteComponents.erase(it);
		return PCX_SUCCESS;
	}

	return PCX_COMPONENT_NOTFOUND;
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

PCX_END_NAMESPACE