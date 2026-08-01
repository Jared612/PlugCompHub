/**
 * @file componentManager.h
 * @brief 组件管理器 `ComponentManager` 的声明。
 * @details
 * 组件管理器是 PCH 内核中「组件类型」的注册表：`componentID` → `Component`（对 `ComponentInfo` 的封装）。
 * `ObjectManager` 等子系统通过 `findComp(componentID)` 取到封装后，再借助其中的 creator/deletor 创建或销毁
 * 具体对象实例；因此本类管的是**元数据与工厂入口**，不直接管理各业务对象的生存期。
 *
 * 主要职责：
 * - 注册 / 批量注册：将插件或内置表导出的 `ComponentInfo` 登记为可查询的 `Component`；
 * - 查询：`getComponentInf`、`findComp`、`getLoadedComponents`；
 * - 注销 / 批量注销：从 `_components` 中移除映射，使后续按 ID 无法再解析到该组件。
 *
 * 并发与数据成员：
 * - `_mutex` 保护 `_components` 与 `_deleteComponents`；
 * - 注销时当前实现将已移除的 `Component*` 放入 `_deleteComponents`，在 `~ComponentManager()` 中与仍注册的项一并
 *   `delete`；即「从注册表立刻不可见，堆上封装对象延迟到管理器析构时释放」。
 */

#pragma once
#include "interface.h"
#include "internal.h"
#include "Component.h"
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>

PCH_BEGIN_NAMESPACE

/**
 * @class ComponentManager
 * @brief 实现 `IComponentManager`，维护组件注册表与延迟删除队列（整体设计见本文件头注释）。
 */
class ComponentManager: public IComponentManager
{
	/**
	 * @brief 组件主存储类型。
	 * @details key 为 `componentID`，value 为组件封装对象指针。
	 */
	using ComponentMap = std::unordered_map<std::string, Component*>;

	/**
	 * @brief 延迟删除队列类型。
	 * @details 用于保存已从注册表移除、待统一释放的组件对象。
	 */
	using DeleteComponentList = std::list<Component*>;

public:
	/**
	 * @brief 构造函数。
	 */
	ComponentManager() = default;

	/**
	 * @brief 析构函数。
	 * @details 释放当前仍注册及延迟删除队列中的组件对象。
	 */
	virtual ~ComponentManager();

	/**
	 * @brief 注册已创建好的组件对象。
	 * @param[in] compInfo 组件信息。
	 * @param[in] comp 组件封装对象。
	 * @return 注册结果错误码。
	 */
	ErrorCode registerComponent(ComponentInfo* compInfo, Component* comp);

	/**
	 * @brief 依据组件信息创建并注册组件。
	 * @param[in] compInfo 组件信息。
	 * @return 注册结果错误码。
	 */
	ErrorCode registerComponent(ComponentInfo* compInfo) override;

	/**
	 * @brief 批量注册组件。
	 * @param[in] cmptable 以 `nullptr` 结尾的组件信息数组。
	 * @return 注册结果错误码。
	 */
	ErrorCode registerComponents(ComponentInfo** cmptable) override;

	/**
	 * @brief 按组件信息注销组件。
	 * @param[in] compInfo 组件信息。
	 * @return 注销结果错误码。
	 */
	ErrorCode unregisterComponent(ComponentInfo* compInfo) override;

	/**
	 * @brief 按组件 ID 注销组件。
	 * @param[in] componentID 组件唯一标识。
	 * @return 注销结果错误码。
	 */
	ErrorCode unregisterComponent(const char* componentID) override;

	/**
	 * @brief 批量注销组件。
	 * @param[in] cmptable 以 `nullptr` 结尾的组件信息数组。
	 * @return 注销结果错误码。
	 */
	ErrorCode unregisterComponents(ComponentInfo** cmptable) override;


	/**
	 * @brief 按组件 ID 获取组件信息。
	 * @param[in] componentID 组件唯一标识。
	 * @return 命中返回组件信息指针，未命中返回 nullptr。
	 */
	ComponentInfo* getComponentInf(const char* componentID) override;

	/**
	 * @brief 按组件 ID 查找组件封装对象。
	 * @param[in] componentID 组件唯一标识。
	 * @return 命中返回组件对象指针，未命中返回 nullptr。
	 */
	Component* findComp(const char* componentID);

	/**
	 * @brief 获取当前已加载组件 ID 列表。
	 * @return 已加载组件 ID 列表。
	 */
	std::list<std::string> getLoadedComponents();

	/**
	 * @brief 从延迟删除队列里把 `compInfo` 对应的包装对象还原到注册表。
	 * @details 用于 `PluginManager::unloadPlugin` 回滚：避免因「unregister 之后再 register」
	 *          路径里调用 `registerComponent(compInfo)` 重新 `new` 一份包装，造成旧包装遗留在
	 *          `_deleteComponents` 里直到管理器析构，既浪费堆也掩盖重复登记错误。
	 * @param[in] compInfo 组件静态信息。
	 * @return PCH_SUCCESS 命中延迟队列并成功回到注册表；
	 *         PCH_COMPONENT_NOTFOUND 延迟队列中没有对应项（调用方可再走常规 `registerComponent`）；
	 *         PCH_COMPONNET_ADYEXIST 同 ID 已在注册表中；其他错误码按参数校验返回。
	 */
	ErrorCode reregisterFromDeleteList(ComponentInfo* compInfo);

private:
	/** @brief 保护 `_components` 与 `_deleteComponents` 的互斥锁。 */
	std::mutex _mutex;

	/** @brief 已注册组件表。 */
	ComponentMap _components;

	/** @brief 待删除组件列表（当前策略为析构时统一回收）。 */
	DeleteComponentList _deleteComponents;
};

PCH_END_NAMESPACE
