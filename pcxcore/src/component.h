/**
 * @file component.h
 * @brief 包装一份 `ComponentInfo`，表示某类组件的元数据与创建入口；由 `ComponentManager` 持有，`ObjectManager` 按此建实例。
 * @details 提供 `getComponentInfo`、`getInfo`；类本身不加锁，由管理器侧保证并发安全。
 */

#pragma once
#include "interface.h"
#include <cstring>

PCX_BEGIN_NAMESPACE

class Component
{
public:
	/**
	 * @brief 构造函数
	 * @param compInfo 组件信息指针
	 */
	explicit Component(ComponentInfo* compInfo)
		: _compInfo(compInfo)
	{
	}

	/**
	 * @brief 获取组件的信息结构体指针
	 * @return 组件信息指针
	 */
	ComponentInfo* getComponentInfo() const
	{
		return _compInfo;
	}

	/**
	 * @brief 按扩展键查询组件元信息字符串
	 * @param key 键名
	 * @return 组件元信息字符串，如果键为空或组件信息为空，返回空串
	 */
	const char* getInfo(const char* key) const
	{
		// 如果键为空或组件信息为空，返回空串
		if (key == nullptr || _compInfo == nullptr)
			return "";

		// 如果键为 "componentID"，返回组件 ID	
		if (std::strcmp(key, "componentID") == 0)		
			return _compInfo->componentID != nullptr ? _compInfo->componentID : "";

		// 如果键为其他，返回空串
		return "";
	}

private:
	ComponentInfo* _compInfo;	// 组件信息指针
};

PCX_END_NAMESPACE