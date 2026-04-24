/**
 * @file objectManager.h
 * @brief 对象管理器头文件，声明对象元信息结构体及对象管理器类。
 * @details
 * 对象管理器是 PCX 框架的核心子系统之一，负责管理所有对象的完整生命周期，
 * 包括具名对象和匿名对象的创建、注册、查找、引用计数查询及销毁。
 * 内部通过名称映射表和地址映射表维护对象的双向索引，
 * 并使用递归互斥锁保证多线程环境下的安全访问。
 */
#pragma once
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "interface.h"
#include "internal.h"
#include "component.h"

PCX_BEGIN_NAMESPACE

/** @brief 对象元信息结构体，记录对象的创建信息。 */
struct ObjectInfo
{
	/** @brief 默认构造函数，将所有字段初始化为安全的零值/空值。 */
	ObjectInfo()
	{
		line      = 0;
		object    = nullptr;
		component = nullptr;
	}

	int         line;       // 创建该对象时的源码行号，用于调试追踪。
	void*       object;     // 对象实例的内存地址。
	Component*  component;  // 对象所属的组件封装指针。
	std::string file;       // 创建该对象时的源码文件路径，用于调试追踪。
	std::string objName;    // 对象名称，具名对象有效；匿名对象此字段为空。
};

/**
 * @brief 根据组件封装创建 ObjectInfo 及其关联的对象实例。
 * @param[in] compInfo 组件封装对象指针，用于获取创建/销毁函数。
 * @param[out] errCode 错误码输出指针，可为 nullptr。
 * @return 成功返回指向新建 ObjectInfo 的指针；失败返回 nullptr。
 */
ObjectInfo* createObjectInfo(Component* component, ErrorCode* errCode);

//对象管理器类，继承自 IObjectManager 接口，提供对象的全生命周期管理。
class ObjectManager: public IObjectManager
{
public:
	/** @brief 具名对象表类型：key 为对象名，value 为对象信息指针。 */
	using RegistedObjInfos = std::unordered_map<std::string, ObjectInfo*>;

	/** @brief 默认构造函数。 */
	ObjectManager()  = default;

	/** @brief 虚析构函数，确保子类正确释放资源。 */
	virtual ~ObjectManager()  = default;

	/**
	 * @brief 创建具名对象（外部接口）。
	 * @details
	 * 创建对象后，如果提供了初始化消息，会通过消息中心向对象派发
	 * SystemObjectInit 初始化事件。
	 * @param[in] componentID 组件 ID，用于定位组件创建工厂。
	 * @param[in] name 对象名称，在系统中必须唯一。
	 * @param[in] initMsg 初始化消息，可为 nullptr 表示无需初始化。
	 * @param[out] errCode 错误码输出，可为 nullptr。
	 * @param[in] file 调用方源码文件路径，可为 nullptr。
	 * @param[in] line 调用方源码行号。
	 * @return 成功返回对象实例地址；失败返回 nullptr。
	 */
	void* createNamedObject(const char* componentID, const char* name, IMessage* initMsg = nullptr, ErrorCode* errCode = nullptr, const char* file = nullptr, int line = 0);

	/**
	 * @brief 创建具名对象（内部版本）。
	 * @details
	 * 不触发初始化消息，返回 ObjectInfo 指针供框架内部使用。
	 * @param[in] componentID 组件 ID。
	 * @param[in] name 对象名称。
	 * @param[out] errCode 错误码输出，可为 nullptr。
	 * @param[in] file 调用方源码文件路径，可为 nullptr。
	 * @param[in] line 调用方源码行号。
	 * @return 成功返回 ObjectInfo 指针；失败返回 nullptr。
	 */
	ObjectInfo* createNamedObject(const char* componentID, const char* name, ErrorCode* errCode = nullptr, const char* file = nullptr, int line = 0);

	/**
	 * @brief 创建匿名对象（外部接口）。
	 * @details
	 * 创建不具名的对象实例，不会注册到名称映射表中。
	 * 如果提供了初始化消息，同样会触发 SystemObjectInit 事件。
	 * @param[in] componentID 组件 ID。
	 * @param[in] initMsg 初始化消息，可为 nullptr。
	 * @param[out] errCode 错误码输出，可为 nullptr。
	 * @param[in] file 调用方源码文件路径，可为 nullptr。
	 * @param[in] line 调用方源码行号。
	 * @return 成功返回对象实例地址；失败返回 nullptr。
	 */
	void* createObject(const char* componentID, IMessage* initMsg = nullptr, ErrorCode* errCode = nullptr, const char* file = nullptr, int line = 0);

	/**
	 * @brief 创建匿名对象（内部版本）。
	 * @details
	 * 不触发初始化消息，返回 ObjectInfo 指针供框架内部使用。
	 * @param[in] componentID 组件 ID。
	 * @param[out] errCode 错误码输出，可为 nullptr。
	 * @param[in] file 调用方源码文件路径，可为 nullptr。
	 * @param[in] line 调用方源码行号。
	 * @return 成功返回 ObjectInfo 指针；失败返回 nullptr。
	 */
	ObjectInfo* createObject(const char* componentID, ErrorCode* errCode = nullptr, const char* file = nullptr, int line = 0);

	/**
	 * @brief 按对象名删除具名对象。
	 * @details
	 * 从名称映射表、地址映射表及对象列表中移除该对象，并释放对象实例内存。
	 * @param[in] objName 要删除的对象名称。
	 * @return 操作结果错误码。
	 */
	ErrorCode deleteObjectByName(const char* objName);

	/**
	 * @brief 按对象地址删除对象。
	 * @details
	 * 在对象列表中查找匹配的地址，移除所有相关索引后释放对象。
	 * 适用于匿名对象和具名对象。
	 * @param[in] obj 对象实例地址。
	 * @param[in] file 调用方源码文件路径，可为 nullptr。
	 * @param[in] line 调用方源码行号。
	 * @return 操作结果错误码。
	 */
	ErrorCode deleteObject(void* obj, const char* file = nullptr, int line = 0);

	/**
	 * @brief 释放由 findObjectByInfo 返回的对象数组。
	 * @param[in] objArray 待释放的对象数组指针。
	 * @return 操作结果错误码。
	 */
	ErrorCode freeObjectArray(IObjectArray* objArray);

	/**
	 * @brief 按名称获取对象元信息。
	 * @param[in] objName 对象名称。
	 * @return 找到返回 ObjectInfo 指针；未找到返回 nullptr。
	 */
	ObjectInfo* getObjInfo(const char* objName);

	/**
	 * @brief 按名称查找对象实例地址。
	 * @param[in] objName 对象名称。
	 * @return 找到返回对象实例地址；未找到返回 nullptr。
	 */
	void* findObject(const char* objName);

	/**
	 * @brief 按组件扩展信息批量过滤对象。
	 * @details
	 * 遍历所有已注册对象，将组件扩展信息与给定的键值对数组逐一匹配，
	 * 所有键值对都匹配的对象会被加入结果数组。
	 * 调用方需使用 freeObjectArray 释放返回的数组。
	 * @param[in] key 扩展信息键名数组。
	 * @param[in] value 扩展信息值数组。
	 * @param[in] count 键值对数量。
	 * @return 匹配的对象数组，调用方需负责释放。
	 */
	IObjectArray* findObjectByInfo(const char* key[], const char* value[], int count);

	/**
	 * @brief 查询对象是否处于已登记状态。
	 * @details PCX 目前未实现自动引用计数；本方法用于判存（对象存在返回 1，否则 0），
	 *          保留 `RefCount` 命名以向前兼容老调用方，含义以「是否已登记」为准。
	 * @param[in] objName 对象名称。
	 * @return 已注册返回 1；未注册或入参非法返回 0。
	 */
	int getRefCount(const char* objName);

	/**
	 * @brief 获取当前所有已注册对象列表的快照副本。
	 * @return 包含所有 ObjectInfo 指针的 vector 副本。
	 */
	std::vector<ObjectInfo*> getRegisterObjects();

	/**
	 * @brief 通过对象实例地址反查对象名称。
	 * @param[in] obj 对象实例地址。
	 * @return 命中具名对象返回其名称；命中匿名对象返回空字符串 ""；
	 *         未命中或 `obj == nullptr` 返回 nullptr（与 `interface.h` 约定一致）。
	 */
	const char* getObjectName(void* obj);

	/**
	 * @brief 将对象注册到具名映射表和地址映射表中。
	 * @details
	 * 同时将对象添加到全局对象列表 _regObjs 中。
	 * 如果对象名已存在则注册失败。
	 * @param[in] objInfo 对象元信息指针。
	 * @param[in] objName 对象名称。
	 * @return 成功返回对象实例地址；名称冲突时返回 nullptr。
	 */
	void* registerObj(ObjectInfo* objInfo, const char* objName);

	/**
	 * @brief 框架退出时批量清理所有对象。
	 * @details
	 * 按注册的逆序销毁对象，跳过核心管理器对象（ObjectManager、
	 * PluginManager、ComponentManager）以避免提前销毁基础设施。
	 * 最后清空所有映射表。
	 */
	void tearDown();

private:
	/**
	 * @brief 统一保护 `_regObjs`、`_regObjsMap`、`_regObjAddrMap` 的递归互斥锁。
	 * @details 合并原 `_regMutex` 与 `_regNamedMutex`，消除两把锁之间的状态不一致窗口；
	 *          保留 recursive 语义以支持从持锁内调用 `registerObj` 等成员。
	 */
	std::recursive_mutex                   _mutex;
	std::vector<ObjectInfo*>               _regObjs;         //所有已注册对象（含具名和匿名）的列表，按注册顺序排列
	RegistedObjInfos                       _regObjsMap;      //对象名 -> ObjectInfo 的映射表，用于按名称快速查找（具名对象）
	std::unordered_map<void*, ObjectInfo*> _regObjAddrMap;   //对象地址 -> ObjectInfo 的映射表，用于按地址反查（具名+匿名）
};

PCX_END_NAMESPACE
