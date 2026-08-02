#include <string.h>
#include <algorithm>
#include <unordered_set>
#include <assert.h>
#include "error.h"
#include "loggerManager.h"
#include "objectManager.h"
#include "messagecenter.h"
#include "componentManager.h"

PCH_BEGIN_NAMESPACE

PCH_REGISTER_COMPONENT(pch::ObjectManager, PCH_OBJECT_MANAGER_ID)

extern ComponentManager* _componentManager;  // 全局组件管理器实例指针
extern MessageCenter* _msgCenter;            // 全局消息中心实例指针

/**
 * @brief 检查给定名称是否属于核心默认对象（不得通过 deleteObject 删除）
 * @details 核心默认对象由 pchcoreStart 创建，由 pchcoreStop 按顺序销毁；
 *          如果业务代码误删，后续框架路径（日志/消息/组件查找）将遭遇空指针/UAF
 */
static bool isCoreDefaultName(const std::string& name)
{
	return name == PCH_DEFAULT_OBJECTMANAGER
		|| name == PCH_DEFAULT_PLUGINMANAGER
		|| name == PCH_DEFAULT_COMPONENTMANAGER
		|| name == PCH_DEFAULT_MESSAGECENTER
		|| name == PCH_DEFAULT_LOGGERMANAGER
		|| name == PCH_DEFAULT_ENVIROMENT;
}

/**
 * @brief 释放对象信息及其关联的对象实例
 * @param[in] objInfo 对象信息指针。外部删除后按约定不应再次使用，或调用方将其置为 nullptr
 */
static void deleteObjInfo(ObjectInfo* objInfo)
{
	if (objInfo == nullptr) {
		return;
	}

	// 当组件 deletor 就绪时，负责销毁实际对象实例；其他路径仅释放 ObjectInfo 自身，
	// 确保无论 deletor 是否可用，都不会泄漏 ObjectInfo
	if (objInfo->component && objInfo->object
		&& objInfo->component->getComponentInfo()
		&& objInfo->component->getComponentInfo()->deletor) {
		try {
			objInfo->component->getComponentInfo()->deletor(objInfo->object);
		} catch (...) {
			WriteLog(LogLevel::Debug, "Delete object [%s] error!", objInfo->objName.c_str());
		}
	}
	objInfo->object = nullptr;
	delete objInfo;
}

/**
 * @brief 根据组件包装创建 ObjectInfo 及其关联的对象实例
 * @param[in] component 组件包装指针，提供 creator/deletor 工厂函数
 * @param[out] errCode 错误码输出指针，可为 nullptr
 * @return 成功返回指向新建 ObjectInfo 的指针；失败返回 nullptr
 */
ObjectInfo* createObjectInfo(Component* component, ErrorCode* errCode)
{
	// 校验组件包装指针
	if (component == nullptr) {
		if (errCode) {
			*errCode = PCH_PARAM_INVALID;
		}
		WriteLog(LogLevel::Fatal, "Component is nullptr");
		return nullptr;
	}

	// 获取组件信息结构体指针，校验组件信息、creator 和 deletor
	ComponentInfo *compInfo = component->getComponentInfo();
	if (compInfo == nullptr || compInfo->creator == nullptr || compInfo->deletor == nullptr) {
		if (errCode) {
			*errCode = PCH_PARAM_INVALID;
		}
		WriteLog(LogLevel::Fatal, "ComponentInfo is nullptr or creator or deletor is nullptr");
		return nullptr;
	}

	// 使用 nothrow 分配 ObjectInfo 结构体，避免 OOM 时抛异常
	ObjectInfo* objInfo = new (std::nothrow) ObjectInfo();
	if (objInfo == nullptr) {
		WriteLog(LogLevel::Fatal, "Alloc ObjectInfo failed, memory may be exhausted");
		if (errCode) {
			*errCode = PCH_OUTOFMEM;
		}
		return nullptr;
	}

	// 关联组件包装指针
	objInfo->component = component;

	// 通过组件 creator 工厂函数创建实际对象实例，校验
	void* obj = compInfo->creator();
	if (obj == nullptr) {
		WriteLog(LogLevel::Fatal, "The pointer is nullptr when the object is created, the memory may be exhausted!!");
		if (errCode) {
			*errCode = PCH_OUTOFMEM;
		}
		// 创建失败，释放已分配的 ObjectInfo 结构体
		delete objInfo;
		return nullptr;
	}
	// 关联对象实例
	objInfo->object = obj;
	return objInfo;
}


// 对象数组类，继承 IObjectArray 接口，提供对象查询和释放功能
class ObjectArray : public IObjectArray
{
public:
	// 存储查询结果中所有对象实例地址的列表
	std::vector<void*> _objects;

	// 获取结果集中的对象数量
	unsigned int GetObjectCount()
	{
		return (unsigned int)_objects.size();
	}

	/**
	 * @brief 按索引从结果集中获取对象实例
	 * @param[in] idx 对象索引
	 * @return 索引有效返回对象地址；越界返回 nullptr
	 */
	void* GetObject(unsigned int idx)
	{
		if (idx >= _objects.size())
		{
			return nullptr;
		}
		return _objects[idx];
	}
};

/**
 * @brief 创建具名对象（外部接口）
 * @param[in] componentID 组件 ID
 * @param[in] objName 对象名称
 * @param[in] initMsg 初始化消息
 * @param[out] errCode 错误码输出
 * @param[in] file 调用方源文件路径
 * @param[in] line 调用方源文件行号
 */
void* ObjectManager::createNamedObject(const char* componentID, const char* objName, IMessage* initMsg, ErrorCode* errCode, const char* file, int line)
{
	// 调用内部版本完成对象创建，返回 ObjectInfo 指针
	ObjectInfo* objInfo = createNamedObject(componentID, objName, errCode, file, line);
	if (objInfo == nullptr) {
		return nullptr;
	}

	// 如果提供初始化消息，通过消息中心直接投递给目标对象
	// 注意：不再假设 initMsg 是本实现的 Message 实例；消息码由调用方在 allocMessage 时设置
	if (initMsg && _msgCenter) {
		// 分发初始化消息期间保留对象，避免并发 deleteObject 造成 UAF
		{
			std::lock_guard<std::recursive_mutex> lk(_mutex);
			objInfo->_inUse++;
		}
		ErrorCode err = _msgCenter->invokeHandler(objInfo, initMsg, nullptr);
		releaseObject(objInfo);
		if (errCode != nullptr)
			*errCode = err;
	}
	return objInfo->object;
}

/**
 * @brief 创建具名对象（内部版本）
 * @details 参数校验 -> 通过组件管理器查找组件 -> 创建对象实例 -> registerObj 持锁写入映射/列表。
 *          重复名称检测集中在 registerObj 中（持锁），外层不做无锁预检
 */
ObjectInfo* ObjectManager::createNamedObject(const char* componentID, const char* objName, ErrorCode* errCode, const char* file, int line)
{
	// 参数校验：组件 ID 和对象名不能为空
	if (componentID == nullptr || componentID[0] == '\0' || objName == nullptr || objName[0] == '\0') {
		WriteLog(LogLevel::Debug, "createNamedObject: invalid parameter");
		if (errCode) {
			*errCode = PCH_PARAM_INVALID;
		}
		return nullptr;
	}

	if (_componentManager == nullptr) {
		WriteLog(LogLevel::Warning, "ComponentManager is nullptr!");
		if (errCode) {
			*errCode = PCH_COMMANAGER_NULLPTR;
		}
		return nullptr;
	}

	Component* comp = _componentManager->findComp(componentID);
	if (comp == nullptr) {
		WriteLog(LogLevel::Debug, "Component[%s] not found", componentID);
		if (errCode) {
			*errCode = PCH_COMPONENT_NOTFOUND;
		}
		return nullptr;
	}

	// 创建对象实例（可能消耗资源）；如果后续 registerObj 发现名称重复，回滚
	ObjectInfo* objInfo = createObjectInfo(comp, errCode);
	if (objInfo == nullptr) {
		return nullptr;
	}

	if (file != nullptr) {
		objInfo->file = file;
		objInfo->line = line;
	}

	void* pobj = registerObj(objInfo, objName);
	if (pobj == nullptr) {
		// 注册失败（名称冲突或分配异常），回滚已创建的对象
		deleteObjInfo(objInfo);
		if (errCode) {
			*errCode = PCH_OBJECT_ADYEXIST;
		}
		return nullptr;
	}

	if (errCode) {
		*errCode = PCH_SUCCESS;
	}
	return objInfo;
}

/**
 * @brief 创建匿名对象（外部接口）
 * @details 先调用内部版本创建对象，再根据 initMsg 触发初始化事件
 */
void* ObjectManager::createObject(const char* componentID, IMessage* initMsg, ErrorCode* errCode, const char* file, int line)
{
	// 委托给内部版本完成对象创建
	ObjectInfo* objInfo = createObject(componentID, errCode, file, line);
	if (objInfo == nullptr) {
		return nullptr;
	}

	// 如果提供初始化消息，分发给对象（消息码由调用方在 allocMessage 时设置）
	if (initMsg && _msgCenter) {
		// 分发初始化消息期间保留对象，避免并发 deleteObject 造成 UAF
		{
			std::lock_guard<std::recursive_mutex> lk(_mutex);
			objInfo->_inUse++;
		}
		ErrorCode err = _msgCenter->invokeHandler(objInfo, initMsg, nullptr);
		releaseObject(objInfo);
		if (errCode != nullptr)
			*errCode = err;
	}
	return objInfo->object;
}

/**
 * @brief 创建匿名对象（内部版本）
 * @details 参数校验 -> 查找组件 -> 创建对象实例 -> 添加到全局列表 _regObjs 和地址映射表 _regObjAddrMap（O(1) 查找/删除）
 */
ObjectInfo* ObjectManager::createObject(const char* componentID, ErrorCode* errCode, const char* file, int line)
{
	// 参数校验：组件 ID 不能为空
	if (componentID == nullptr || componentID[0] == '\0') {
		WriteLog(LogLevel::Debug, "createObject: componentID is nullptr or empty");
		if (errCode) {
			*errCode = PCH_PARAM_INVALID;
		}
		return nullptr;
	}

	// 组件管理器必须已初始化
	if (_componentManager == nullptr) {
		WriteLog(LogLevel::Warning, "ComponentManager is nullptr!");
		if (errCode) {
			*errCode = PCH_COMMANAGER_NULLPTR;
		}
		return nullptr;
	}

	// 按组件 ID 查找组件包装
	Component* comp = _componentManager->findComp(componentID);
	if (comp == nullptr) {
		WriteLog(LogLevel::Debug, "Component[%s] not found", componentID);
		if (errCode) {
			*errCode = PCH_COMPONENT_NOTFOUND;
		}
		return nullptr;
	}

	// 创建 ObjectInfo 和实际对象实例
	ObjectInfo* objInfo = createObjectInfo(comp, errCode);
	if (objInfo == nullptr) {
		return nullptr;
	}

	// 记录调用方源位置
	if (file != nullptr) {
		objInfo->file = file;
		objInfo->line = line;
	}

	try {
		std::lock_guard<std::recursive_mutex> lk(_mutex);
		_regObjs.push_back(objInfo);
		// 匿名对象也加入地址表，deleteObject(void*) / getObjectName(void*) 均为 O(1)
		if (objInfo->object) {
			_regObjAddrMap[objInfo->object] = objInfo;
		}
	} catch (...) {
		// 回滚：移除可能已加入 _regObjs 的条目，避免悬垂指针残留在列表中
		{
			std::lock_guard<std::recursive_mutex> lk(_mutex);
			if (!_regObjs.empty() && _regObjs.back() == objInfo) {
				_regObjs.pop_back();
			}
			if (objInfo->object) {
				_regObjAddrMap.erase(objInfo->object);
			}
		}
		// 异常如 vector 扩容失败：回滚对象实例和 ObjectInfo
		deleteObjInfo(objInfo);
		if (errCode) {
			*errCode = PCH_OUTOFMEM;
		}
		return nullptr;
	}

	if (errCode) {
		*errCode = PCH_SUCCESS;
	}
	return objInfo;
}

/**
 * @brief 将对象注册到名称映射表、地址映射表和全局对象列表
 * @details 注册持有统一 _mutex，在同一临界区内完成映射表和列表写入，
 *          保证三个索引（名称/地址/列表）的一致性
 */
void* ObjectManager::registerObj(ObjectInfo* objInfo, const char* objName)
{
	assert(objInfo);
	assert(objName);

	objInfo->objName = objName;

	{
		std::lock_guard<std::recursive_mutex> lk(_mutex);
		// O(1) 重复名称检查
		if (_regObjsMap.find(objName) != _regObjsMap.end()) {
			WriteLog(LogLevel::Warning, "Object[%s] already created", objName);
			return nullptr;
		}
		// 同时写入名称映射表、地址映射表和全局对象列表，单锁保证索引一致性
		_regObjsMap[objName] = objInfo;
		if (objInfo->object) {
			_regObjAddrMap[objInfo->object] = objInfo;
		}
		_regObjs.push_back(objInfo);
	}

	assert(objInfo->component != nullptr);
	assert(objInfo->component->getComponentInfo() != nullptr);
	assert(objInfo->component->getComponentInfo()->componentID != nullptr);
	WriteLog(LogLevel::Trace, "Create component [%s], object name: [%s] succeed!",
			 objInfo->component->getComponentInfo()->componentID, objName);
	return objInfo->object;
}

/**
 * @brief 按对象名删除具名对象
 * @details 三步操作：从名称映射表和地址映射表移除 -> 从全局对象列表移除 ->
 *          调用 deleteObject 释放对象实例内存。
 *          每一步独立持锁，减少锁持有时间
 */
ErrorCode ObjectManager::deleteObjectByName(const char* objName)
{
	// 参数校验：对象名不能为空
	if (objName == nullptr || objName[0] == '\0') {
		WriteLog(LogLevel::Debug, "deleteObjectByName: objName is nullptr or empty");
		return PCH_PARAM_INVALID;
	}

	// 防护：拒绝通过业务接口删除核心默认对象，避免引起框架级 UAF
	if (isCoreDefaultName(objName)) {
		WriteLog(LogLevel::Warning, "deleteObjectByName: refuse to delete core default object [%s]", objName);
		return PCH_NOTALLOW;
	}

	ObjectInfo* objInfo = nullptr;
	bool deferred = false;
	{
		std::lock_guard<std::recursive_mutex> lk(_mutex);

		auto it = _regObjsMap.find(objName);
		if (it == _regObjsMap.end()) {
			return PCH_OBJECT_NOTFOUND;
		}
		if (it->second == nullptr) {
			_regObjsMap.erase(it);
			return PCH_OBJECT_NULLPTR;
		}

		objInfo = it->second;
		_regObjsMap.erase(it);

		// 同步移除地址映射
		if (objInfo->object) {
			auto addrit = _regObjAddrMap.find(objInfo->object);
			if (addrit != _regObjAddrMap.end())
				_regObjAddrMap.erase(addrit);
		}

		// 从全局对象列表中移除（按 ObjectInfo 地址匹配，避免与同地址对象混淆）
		auto itv = std::find(_regObjs.begin(), _regObjs.end(), objInfo);
		if (itv != _regObjs.end()) {
			_regObjs.erase(itv);
		}

		// 使用中：标记待删除，等最后一个使用者 releaseObject 时实际销毁
		if (objInfo->_inUse > 0) {
			objInfo->_pendingDelete = true;
			_pendingDeleteObjs.push_back(objInfo);
			deferred = true;
		}
	}

	WriteLog(LogLevel::Trace, "Object [%s] deleted%s", objName, deferred ? " (deferred)" : "");
	// 已从所有索引/列表中移除；未在使用中则直接销毁对象和 ObjectInfo
	if (!deferred) {
		deleteObjInfo(objInfo);
	}

	return PCH_SUCCESS;
}

/**
 * @brief 通过对象实例地址反查对象名称
 * @param[in] obj 对象实例地址
 * @return 对象名称
 */
const char* ObjectManager::getObjectName(void* obj)
{
	if (obj == nullptr) {
		WriteLog(LogLevel::Debug, "getObjectName: obj is nullptr");
		return nullptr;
	}

	std::lock_guard<std::recursive_mutex> lk(_mutex);

	auto it = _regObjAddrMap.find(obj);
	if (it != _regObjAddrMap.end() && it->second != nullptr) {
		// 匿名对象 objName 为空；具名对象返回对应名称
		return it->second->objName.c_str();
	}
	// 未知指针；interface.h 注释说返回 nullptr
	return nullptr;
}

/**
 * @brief 框架退出时批量清理所有对象
 * @details 按注册逆序销毁对象，跳过核心管理器对象（ObjectManager、PluginManager、ComponentManager）
 *          避免提前销毁基础设施。最后清空所有映射表
 */
void ObjectManager::tearDown()
{
	// 按创建逆序销毁业务对象；同时保留 ObjectManager/PluginManager/ComponentManager
	// 的 ObjectInfo 在 _regObjs 中，供 pchcoreStop 按顺序销毁 —— 避免 DLL 卸载
	// 时 ComponentInfo* 指向已释放内存
	std::lock_guard<std::recursive_mutex> lk(_mutex);

	std::vector<ObjectInfo*> preserved;
	for (auto it = _regObjs.rbegin(); it != _regObjs.rend(); ++it) {
		if (*it == nullptr) {
			continue;
		}
		const std::string& n = (*it)->objName;
		if (n == PCH_DEFAULT_OBJECTMANAGER
			|| n == PCH_DEFAULT_PLUGINMANAGER
			|| n == PCH_DEFAULT_COMPONENTMANAGER) {
			preserved.push_back(*it);
			continue;
		}
		deleteObjInfo(*it);
	}
	_regObjs.clear();
	// 维持原始注册顺序（preserved 是逆序收集的）
	for (auto it = preserved.rbegin(); it != preserved.rend(); ++it) {
		_regObjs.push_back(*it);
	}

	// 仅保留核心管理器自身的映射，方便 pchcoreStop 按名称查找并销毁
	RegistedObjInfos keepByName;
	std::unordered_map<void*, ObjectInfo*> keepByAddr;
	for (auto* oi : preserved) {
		if (oi == nullptr) {
			continue;
		}
		keepByName[oi->objName] = oi;
		if (oi->object) {
			keepByAddr[oi->object] = oi;
		}
	}
	_regObjsMap.swap(keepByName);
	_regObjAddrMap.swap(keepByAddr);

	// 清理等待延迟删除的对象（框架退出：无论是否仍在使用，直接销毁）
	for (auto* oi : _pendingDeleteObjs) {
		if (oi) {
			deleteObjInfo(oi);
		}
	}
	_pendingDeleteObjs.clear();
}

/**
 * @brief 按对象地址删除对象
 * @param[in] obj 对象实例地址
 * @param[in] file 调用方源文件路径
 * @param[in] line 调用方源文件行号
 * @return 操作结果错误码
 */
ErrorCode ObjectManager::deleteObject(void* obj, const char* file, int line)
{
	(void)file;
	(void)line;

	// 校验对象实例地址
	if (obj == nullptr) {
		return PCH_PARAM_NULLPTR;
	}

	ObjectInfo* objInfo = nullptr;
	bool deferred = false;
	{
		std::lock_guard<std::recursive_mutex> lk(_mutex);

		// O(1) 地址表查找
		auto addrit = _regObjAddrMap.find(obj);
		if (addrit == _regObjAddrMap.end()) {
			return PCH_OBJECT_NOTFOUND;
		}
		objInfo = addrit->second;

		// 防护：禁止按地址删除核心默认对象；只读检查，不修改任何表
		if (objInfo && isCoreDefaultName(objInfo->objName)) {
			WriteLog(LogLevel::Warning, "deleteObject: refuse to delete core default object [%s]", objInfo->objName.c_str());
			return PCH_NOTALLOW;
		}

		_regObjAddrMap.erase(addrit);

		// 具名对象：同步清除名称表
		if (objInfo && !objInfo->objName.empty()) {
			auto it = _regObjsMap.find(objInfo->objName);
			if (it != _regObjsMap.end() && it->second == objInfo) {
				_regObjsMap.erase(it);
			}
		}

		// 从全局对象列表中移除
		if (objInfo) {
			auto itv = std::find(_regObjs.begin(), _regObjs.end(), objInfo);
			if (itv != _regObjs.end()) {
				_regObjs.erase(itv);
			}

			// 使用中：标记待删除，等最后一个使用者 releaseObject 时实际销毁
			if (objInfo->_inUse > 0) {
				objInfo->_pendingDelete = true;
				_pendingDeleteObjs.push_back(objInfo);
				deferred = true;
			}
		}
	}

	// 释放对象实例和 ObjectInfo 结构体（不加锁）；使用中的对象延迟到 releaseObject
	if (!deferred) {
		deleteObjInfo(objInfo);
	}
	return PCH_SUCCESS;
}

/**
 * @brief 释放对象数组
 * @param[in] objArray 要释放的对象数组
 * @return 操作结果错误码
 */
ErrorCode ObjectManager::freeObjectArray(IObjectArray* objArray)
{
	// IObjectArray 具有虚析构函数，可通过接口指针 delete 正确析构派生类
	if (objArray) {
		delete objArray;
		return PCH_SUCCESS;
	}
	return PCH_PARAM_NULLPTR;
}

/**
 * @brief 按名称获取对象元信息
 * @param[in] objName 对象名称
 * @return 找到返回 ObjectInfo 指针；未找到返回 nullptr
 */
ObjectInfo* ObjectManager::getObjInfo(const char* objName)
{
	// 校验对象名称
	if (objName == nullptr || objName[0] == '\0') {
		WriteLog(LogLevel::Debug, "getObjInfo: objName is nullptr or empty");
		return nullptr;
	}

	std::lock_guard<std::recursive_mutex> lk(_mutex);
	auto it = _regObjsMap.find(objName);
	if (it == _regObjsMap.end()) {
		WriteLog(LogLevel::Trace, "Object [%s] not found", objName);
		return nullptr;
	}
	return it->second;
}

/**
 * @brief 按名称获取对象并标记为使用中（in-use 计数 +1）
 * @details 与 getObjInfo 的区别：返回前对对象加"使用中"计数；此后即使被 deleteObject，
 *          实际销毁也会延迟到 releaseObject 计数归零，避免分发过程中 UAF
 */
ObjectInfo* ObjectManager::getObjInfoForUse(const char* objName)
{
	if (objName == nullptr || objName[0] == '\0') {
		return nullptr;
	}

	std::lock_guard<std::recursive_mutex> lk(_mutex);
	auto it = _regObjsMap.find(objName);
	if (it == _regObjsMap.end() || it->second == nullptr || it->second->_pendingDelete) {
		return nullptr;
	}
	++it->second->_inUse;
	return it->second;
}

/**
 * @brief 释放一次"使用中"标记；若计数归零且对象曾被标记待删除，则在此完成实际销毁
 */
void ObjectManager::releaseObject(ObjectInfo* objInfo)
{
	if (objInfo == nullptr) {
		return;
	}

	ObjectInfo* toDelete = nullptr;
	{
		std::lock_guard<std::recursive_mutex> lk(_mutex);
		if (objInfo->_inUse > 0) {
			--objInfo->_inUse;
		}
		if (objInfo->_inUse == 0 && objInfo->_pendingDelete) {
			auto it = std::find(_pendingDeleteObjs.begin(), _pendingDeleteObjs.end(), objInfo);
			if (it != _pendingDeleteObjs.end()) {
				_pendingDeleteObjs.erase(it);
			}
			toDelete = objInfo;
		}
	}

	if (toDelete) {
		deleteObjInfo(toDelete);
	}
}

/**
 * @brief 按名称查找对象实例地址
 * @param[in] objName 对象名称
 * @return 找到返回对象实例地址；未找到返回 nullptr
 */
void* ObjectManager::findObject(const char* objName)
{
	// 校验对象名称
	if (objName == nullptr || objName[0] == '\0') {
		WriteLog(LogLevel::Debug, "findObject: objName is nullptr or empty");
		return nullptr;
	}

	std::lock_guard<std::recursive_mutex> lk(_mutex);
	auto it = _regObjsMap.find(objName);
	if (it == _regObjsMap.end() || it->second == nullptr) {
		return nullptr;
	}
	return it->second->object;
}

/**
 * @brief 按组件扩展信息批量过滤对象
 * @param[in] key 扩展信息键名数组
 * @param[in] value 扩展信息值数组
 * @param[in] count 键值对数量
 * @return 匹配的对象数组，调用方负责释放
 */
IObjectArray* ObjectManager::findObjectByInfo(const char* key[], const char* value[], int count)
{
	if (key == nullptr || value == nullptr || count <= 0) {
		WriteLog(LogLevel::Debug, "key or value is nullptr");
		return nullptr;
	}

	ObjectArray* objArray = new (std::nothrow) ObjectArray();
	if (objArray == nullptr) {
		WriteLog(LogLevel::Fatal, "findObjectByInfo: alloc ObjectArray failed");
		return nullptr;
	}
	{
		std::lock_guard<std::recursive_mutex> lk(_mutex);
		for (auto* objInfo : _regObjs) {
			if (objInfo == nullptr || objInfo->component == nullptr) {
				continue;
			}
			bool isMatch = true;
			for (int k = 0; k < count; k++) {
				if (key[k] == nullptr || value[k] == nullptr) {
					isMatch = false;
					break;
				}
				const char* info = objInfo->component->getInfo(key[k]);
				if (info == nullptr || strcmp(info, value[k]) != 0) {
					isMatch = false;
					break;
				}
			}
			if (isMatch) {
				objArray->_objects.push_back(objInfo->object);
			}
		}
	}
	return objArray;
}

/**
 * @brief 查询对象是否处于已登记状态
 * @details PCH 目前未实现自动引用计数；ObjectInfo::counter 字段始终为 1。
 *          语义已退化为"存在性检查"，此处直接根据命中/未命中返回 1/0
 * @param[in] objName 对象名称
 * @return 已登记返回 1；未登记或输入非法返回 0
 */
int ObjectManager::getRefCount(const char* objName)
{
	if (objName == nullptr || objName[0] == '\0') {
		WriteLog(LogLevel::Debug, "getRefCount: objName is nullptr or empty");
		return 0;
	}
	ObjectInfo* objInfo = getObjInfo(objName);
	return objInfo ? 1 : 0;
}

/**
 * @brief 获取当前所有已注册对象列表的快照副本
 * @return 成功返回当前所有已注册对象的列表
 */
std::vector<ObjectInfo*> ObjectManager::getRegisterObjects()
{
	std::lock_guard<std::recursive_mutex> lk(_mutex);
	return _regObjs;
}

/**
 * @brief 快照所有已注册对象并逐个加 in-use 计数，防止遍历分发期间被并发删除
 */
std::vector<ObjectInfo*> ObjectManager::getRegisterObjectsForUse()
{
	std::lock_guard<std::recursive_mutex> lk(_mutex);
	std::vector<ObjectInfo*> result;
	result.reserve(_regObjs.size());
	for (auto* oi : _regObjs) {
		if (oi && !oi->_pendingDelete) {
			++oi->_inUse;
			result.push_back(oi);
		}
	}
	return result;
}

PCH_END_NAMESPACE
