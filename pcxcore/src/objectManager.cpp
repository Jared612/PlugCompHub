#include <string.h>
#include <algorithm>
#include <unordered_set>
#include <assert.h>
#include "error.h"
#include "loggerManager.h"
#include "objectManager.h"
#include "messagecenter.h"
#include "componentManager.h"

PCX_BEGIN_NAMESPACE

PCX_REGISTER_COMPONENT(pcx::ObjectManager, PCX_OBJECT_MANAGER_ID)

extern ComponentManager* _componentManager;  //全局组件管理器实例指针
extern MessageCenter* _msgCenter;            //全局消息中心实例指针

/**
 * @brief 判断给定名称是否属于核心默认对象（严禁通过 `deleteObject*` 删除）。
 * @details 核心默认对象由 `pcxcoreStart` 创建、`pcxcoreStop` 按序销毁；
 *          若业务代码误删，会令后续框架路径（日志/消息/组件查找）走入空指针或 UAF。
 */
static bool isCoreDefaultName(const std::string& name)
{
	return name == PCX_DEFAULT_OBJECTMANAGER
		|| name == PCX_DEFAULT_PLUGINMANAGER
		|| name == PCX_DEFAULT_COMPONENTMANAGER
		|| name == PCX_DEFAULT_MESSAGECENTER
		|| name == PCX_DEFAULT_LOGGERMANAGER
		|| name == PCX_DEFAULT_ENVIROMENT;
}

/**
 * @brief 释放对象信息及其关联的对象实例。
 * @param[in] objInfo 对象信息指针。外部调用删除后应按约定不要再使用，或自己置 nullptr
 */
static void deleteObjInfo(ObjectInfo* objInfo)
{
	if (objInfo == nullptr) {
		return;
	}

	// 组件及 deletor 均就绪时，负责销毁实际对象实例；其余路径只释放 ObjectInfo 自身，保证无论 deletor 是否可用都不会泄漏 ObjectInfo
	if (objInfo->component && objInfo->object
		&& objInfo->component->getComponentInfo()
		&& objInfo->component->getComponentInfo()->deletor) {
		try {
			objInfo->component->getComponentInfo()->deletor(objInfo->object);
		} catch (...) {
			WriteLog(PcxLogLevel::Debug, "Delete object [%s] error!", objInfo->objName.c_str());
		}
	}
	objInfo->object = nullptr;
	delete objInfo;
}

/**
 * @brief 根据组件封装创建 ObjectInfo 及其关联的对象实例。
 * @param[in] component 组件封装指针，提供 creator/deletor 工厂函数。
 * @param[out] errCode 错误码输出指针，可为 nullptr。
 * @return 成功返回 ObjectInfo 指针；失败返回 nullptr。
 */
ObjectInfo* createObjectInfo(Component* component, ErrorCode* errCode)
{
	// 校验组件封装指针是否为空
	if (component == nullptr) {
		if (errCode) {
			*errCode = PCX_PARAM_INVALID;
		}
		WriteLog(PcxLogLevel::Fatal, "Component is nullptr");
		return nullptr;
	}

	// 获取组件信息结构体指针，校验组件信息结构体指针是否为空，组件创建函数和销毁函数是否为空
	ComponentInfo *compInfo = component->getComponentInfo();
	if (compInfo == nullptr || compInfo->creator == nullptr || compInfo->deletor == nullptr) {
		if (errCode) {
			*errCode = PCX_PARAM_INVALID;
		}
		WriteLog(PcxLogLevel::Fatal, "ComponentInfo is nullptr or creator or deletor is nullptr");
		return nullptr;
	}

	// 使用 nothrow 分配 ObjectInfo 结构体，避免内存不足时抛异常
	ObjectInfo* objInfo = new (std::nothrow) ObjectInfo();
	if (objInfo == nullptr) {
		WriteLog(PcxLogLevel::Fatal, "Alloc ObjectInfo failed, memory may be exhausted");
		if (errCode) {
			*errCode = PCX_OUTOFMEM;
		}
		return nullptr;
	}

	// 关联组件封装指针
	objInfo->component = component;

	// 通过组件的 creator 工厂函数创建实际对象实例，校验对象实例是否为空
	void* obj = compInfo->creator();
	if (obj == nullptr) {
		WriteLog(PcxLogLevel::Fatal, "The pointer is nullptr when the object is created, the memory may be exhausted!!");
		if (errCode) {
			*errCode = PCX_OUTOFMEM;
		}
		// 创建失败，释放已分配的 ObjectInfo 结构体
		delete objInfo;
		return nullptr;
	}
	// 关联对象实例
	objInfo->object = obj;
	return objInfo;
}


//对象数组类，继承自 IObjectArray 接口，提供对象的查询和释放功能
class ObjectArray : public IObjectArray
{
public:
	//存储查询结果中所有对象实例地址的列表
	std::vector<void*> _objects;

	//获取结果集中的对象数量
	unsigned int GetObjectCount()
	{
		return (unsigned int)_objects.size();
	}

	/**
	 * @brief 按索引获取结果集中的对象实例。
	 * @param[in] idx 对象索引。
	 * @return 索引有效返回对象地址；越界返回 nullptr。
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
 * @brief 创建具名对象（外部接口）。
 * @param[in] componentID 组件 ID。
 * @param[in] objName 对象名称。
 * @param[in] initMsg 初始化消息。
 * @param[out] errCode 错误码输出。
 * @param[in] file 调用方源码文件路径。
 * @param[in] line 调用方源码行号。
 */
void* ObjectManager::createNamedObject(const char* componentID, const char* objName, IMessage* initMsg, ErrorCode* errCode, const char* file, int line)
{
	// 调用内部版本完成对象创建，返回 ObjectInfo 指针
	ObjectInfo* objInfo = createNamedObject(componentID, objName, errCode, file, line);
	if (objInfo == nullptr) {
		return nullptr;
	}

	// 如果提供了初始化消息，通过消息中心直接投递给目标对象
	// 注：不再假设 initMsg 为本实现 Message 的实例；消息码由调用方在 allocMessage 时设置
	if (initMsg && _msgCenter) {
		ErrorCode err = _msgCenter->invokeHandler(objInfo, initMsg, nullptr);
		if (errCode != nullptr)
			*errCode = err;
	}
	return objInfo->object;
}

/**
 * @brief 创建具名对象（内部版本）。
 * @details 参数校验 -> 通过组件管理器查找组件 -> 创建对象实例 -> `registerObj` 持锁写入映射/列表。
 *          重名检测集中在 `registerObj` 内部（持锁），不再在外层做无锁预检。
 */
ObjectInfo* ObjectManager::createNamedObject(const char* componentID, const char* objName, ErrorCode* errCode, const char* file, int line)
{
	// 参数校验：组件 ID 和对象名不能为空
	if (componentID == nullptr || componentID[0] == '\0' || objName == nullptr || objName[0] == '\0') {
		WriteLog(PcxLogLevel::Debug, "createNamedObject: invalid parameter");
		if (errCode) {
			*errCode = PCX_PARAM_INVALID;
		}
		return nullptr;
	}

	if (_componentManager == nullptr) {
		WriteLog(PcxLogLevel::Warning, "ComponentManager is nullptr!");
		if (errCode) {
			*errCode = PCX_COMMANAGER_NULLPTR;
		}
		return nullptr;
	}

	Component* comp = _componentManager->findComp(componentID);
	if (comp == nullptr) {
		WriteLog(PcxLogLevel::Debug, "Component[%s] not found", componentID);
		if (errCode) {
			*errCode = PCX_COMPONENT_NOTFOUND;
		}
		return nullptr;
	}

	// 创建对象实例（可能消耗资源）；若后续 registerObj 发现重名再回滚
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
			*errCode = PCX_OBJECT_ADYEXIST;
		}
		return nullptr;
	}

	if (errCode) {
		*errCode = PCX_SUCCESS;
	}
	return objInfo;
}

/**
 * @brief 创建匿名对象（外部接口）。
 * @details
 * 先调用内部版本创建对象，然后根据 initMsg 触发初始化事件。
 */
void* ObjectManager::createObject(const char* componentID, IMessage* initMsg, ErrorCode* errCode, const char* file, int line)
{
	// 委托内部版本完成对象创建
	ObjectInfo* objInfo = createObject(componentID, errCode, file, line);
	if (objInfo == nullptr) {
		return nullptr;
	}

	// 如果提供了初始化消息，向对象派发（消息码由调用方在 allocMessage 时设置）
	if (initMsg && _msgCenter) {
		ErrorCode err = _msgCenter->invokeHandler(objInfo, initMsg, nullptr);
		if (errCode != nullptr)
			*errCode = err;
	}
	return objInfo->object;
}

/**
 * @brief 创建匿名对象（内部版本）。
 * @details 参数校验 -> 查找组件 -> 创建对象实例 -> 同时加入全局列表 `_regObjs` 与地址映射 `_regObjAddrMap`（O(1) 查找/删除）。
 */
ObjectInfo* ObjectManager::createObject(const char* componentID, ErrorCode* errCode, const char* file, int line)
{
	// 参数校验：组件 ID 不能为空
	if (componentID == nullptr || componentID[0] == '\0') {
		WriteLog(PcxLogLevel::Debug, "createObject: componentID is nullptr or empty");
		if (errCode) {
			*errCode = PCX_PARAM_INVALID;
		}
		return nullptr;
	}

	// 组件管理器必须已初始化
	if (_componentManager == nullptr) {
		WriteLog(PcxLogLevel::Warning, "ComponentManager is nullptr!");
		if (errCode) {
			*errCode = PCX_COMMANAGER_NULLPTR;
		}
		return nullptr;
	}

	// 通过组件 ID 查找组件封装
	Component* comp = _componentManager->findComp(componentID);
	if (comp == nullptr) {
		WriteLog(PcxLogLevel::Debug, "Component[%s] not found", componentID);
		if (errCode) {
			*errCode = PCX_COMPONENT_NOTFOUND;
		}
		return nullptr;
	}

	// 创建 ObjectInfo 和实际对象实例
	ObjectInfo* objInfo = createObjectInfo(comp, errCode);
	if (objInfo == nullptr) {
		return nullptr;
	}

	// 记录调用方的源码位置
	if (file != nullptr) {
		objInfo->file = file;
		objInfo->line = line;
	}

	try {
		std::lock_guard<std::recursive_mutex> lk(_mutex);
		_regObjs.push_back(objInfo);
		// 匿名对象也纳入地址表，`deleteObject(void*)` / `getObjectName(void*)` 均可 O(1)
		if (objInfo->object) {
			_regObjAddrMap[objInfo->object] = objInfo;
		}
	} catch (...) {
		// vector 扩容失败等异常：回滚对象实例与 ObjectInfo
		deleteObjInfo(objInfo);
		if (errCode) {
			*errCode = PCX_OUTOFMEM;
		}
		return nullptr;
	}

	if (errCode) {
		*errCode = PCX_SUCCESS;
	}
	return objInfo;
}

/**
 * @brief 将对象注册到具名映射表、地址映射表及全局对象列表中。
 * @details
 * 注册过程持有统一的 `_mutex`，在同一临界区内完成映射表与列表的写入，
 * 确保三索引（name / addr / list）一致。
 */
void* ObjectManager::registerObj(ObjectInfo* objInfo, const char* objName)
{
	assert(objInfo);
	assert(objName);

	objInfo->objName = objName;

	{
		std::lock_guard<std::recursive_mutex> lk(_mutex);
		// O(1) 重名检测
		if (_regObjsMap.find(objName) != _regObjsMap.end()) {
			WriteLog(PcxLogLevel::Warning, "Object[%s] already created", objName);
			return nullptr;
		}
		// 同时写入名称映射、地址映射与全局对象列表，单锁内完成保证索引一致
		_regObjsMap[objName] = objInfo;
		if (objInfo->object) {
			_regObjAddrMap[objInfo->object] = objInfo;
		}
		_regObjs.push_back(objInfo);
	}

	assert(objInfo->component != nullptr);
	assert(objInfo->component->getComponentInfo() != nullptr);
	assert(objInfo->component->getComponentInfo()->componentID != nullptr);
	WriteLog(PcxLogLevel::Trace, "Create component [%s], object name: [%s] succeed!",
			 objInfo->component->getComponentInfo()->componentID, objName);
	return objInfo->object;
}

/**
 * @brief 按对象名删除具名对象。
 * @details
 * 分三步操作：从具名映射表和地址映射表中移除 -> 从全局对象列表中移除 ->
 * 调用 deleteObject 释放对象实例内存。
 * 每步操作独立加锁，减少锁持有时间。
 */
ErrorCode ObjectManager::deleteObjectByName(const char* objName)
{
	// 参数校验：对象名不能为空
	if (objName == nullptr || objName[0] == '\0') {
		WriteLog(PcxLogLevel::Debug, "deleteObjectByName: objName is nullptr or empty");
		return PCX_PARAM_INVALID;
	}

	// 防呆：拒绝通过业务接口删除核心默认对象，避免引发框架级 UAF
	if (isCoreDefaultName(objName)) {
		WriteLog(PcxLogLevel::Warning, "deleteObjectByName: refuse to delete core default object [%s]", objName);
		return PCX_NOTALLOW;
	}

	ObjectInfo* objInfo = nullptr;
	{
		std::lock_guard<std::recursive_mutex> lk(_mutex);

		auto it = _regObjsMap.find(objName);
		if (it == _regObjsMap.end()) {
			return PCX_OBJECT_NOTFOUND;
		}
		if (it->second == nullptr) {
			_regObjsMap.erase(it);
			return PCX_OBJECT_NULLPTR;
		}

		objInfo = it->second;
		_regObjsMap.erase(it);

		// 同步移除地址映射
		if (objInfo->object) {
			auto addrit = _regObjAddrMap.find(objInfo->object);
			if (addrit != _regObjAddrMap.end())
				_regObjAddrMap.erase(addrit);
		}

		// 从全局对象列表中移除（按 ObjectInfo 地址匹配，避免被同地址对象混淆）
		auto itv = std::find(_regObjs.begin(), _regObjs.end(), objInfo);
		if (itv != _regObjs.end()) {
			_regObjs.erase(itv);
		}
	}

	WriteLog(PcxLogLevel::Trace, "Object [%s] deleted", objName);
	// 已从所有索引/列表移除，直接销毁对象与 ObjectInfo
	deleteObjInfo(objInfo);

	return PCX_SUCCESS;
}

/**
 * @brief 通过对象实例地址反查对象名称。
 * @param[in] obj 对象实例地址。
 * @return 对象名称。
 */
const char* ObjectManager::getObjectName(void* obj)
{
	if (obj == nullptr) {
		WriteLog(PcxLogLevel::Debug, "getObjectName: obj is nullptr");
		return nullptr;
	}

	std::lock_guard<std::recursive_mutex> lk(_mutex);

	auto it = _regObjAddrMap.find(obj);
	if (it != _regObjAddrMap.end() && it->second != nullptr) {
		// 匿名对象 objName 为空字符串；具名对象返回对应名称
		return it->second->objName.c_str();
	}
	// 未知指针按 interface.h 注释返回 nullptr
	return nullptr;
}

/**
 * @brief 框架退出时批量清理所有对象。
 * @details
 * 按注册的逆序销毁对象，跳过核心管理器对象（ObjectManager、PluginManager、ComponentManager）以避免提前销毁基础设施。
 * 最后清空所有映射表。
 */
void ObjectManager::tearDown()
{
	// 按照创建的逆序销毁业务对象；同时把 ObjectManager/PluginManager/ComponentManager
	// 的 ObjectInfo 保留在 _regObjs 中供 pcxcoreStop 按顺序销毁——避免 DLL 卸载后
	// ComponentInfo* 指向已释放内存。
	std::lock_guard<std::recursive_mutex> lk(_mutex);

	std::vector<ObjectInfo*> preserved;
	for (auto it = _regObjs.rbegin(); it != _regObjs.rend(); ++it) {
		if (*it == nullptr) {
			continue;
		}
		const std::string& n = (*it)->objName;
		if (n == PCX_DEFAULT_OBJECTMANAGER
			|| n == PCX_DEFAULT_PLUGINMANAGER
			|| n == PCX_DEFAULT_COMPONENTMANAGER) {
			preserved.push_back(*it);
			continue;
		}
		deleteObjInfo(*it);
	}
	_regObjs.clear();
	// 维持原注册顺序（preserved 目前是反向收集的）
	for (auto it = preserved.rbegin(); it != preserved.rend(); ++it) {
		_regObjs.push_back(*it);
	}

	// 只保留核心管理器自身的映射，方便 pcxcoreStop 按名查找并销毁
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
}

/**
 * @brief 按对象地址删除对象。
 * @param[in] obj 对象实例地址。
 * @param[in] file 调用方源码文件路径。
 * @param[in] line 调用方源码行号。
 * @return 操作结果错误码。
 */
ErrorCode ObjectManager::deleteObject(void* obj, const char* file, int line)
{
	(void)file;
	(void)line;

	// 校验对象实例地址是否为空
	if (obj == nullptr) {
		return PCX_PARAM_NULLPTR;
	}

	ObjectInfo* objInfo = nullptr;
	{
		std::lock_guard<std::recursive_mutex> lk(_mutex);

		// O(1) 地址表查找
		auto addrit = _regObjAddrMap.find(obj);
		if (addrit == _regObjAddrMap.end()) {
			return PCX_OBJECT_NOTFOUND;
		}
		objInfo = addrit->second;

		// 防呆：禁止通过地址删除核心默认对象；只读检查，不改动任何表
		if (objInfo && isCoreDefaultName(objInfo->objName)) {
			WriteLog(PcxLogLevel::Warning, "deleteObject: refuse to delete core default object [%s]", objInfo->objName.c_str());
			return PCX_NOTALLOW;
		}

		_regObjAddrMap.erase(addrit);

		// 具名对象同步清名称表
		if (objInfo && !objInfo->objName.empty()) {
			auto it = _regObjsMap.find(objInfo->objName);
			if (it != _regObjsMap.end() && it->second == objInfo) {
				_regObjsMap.erase(it);
			}
		}

		// 从全局对象列表移除
		if (objInfo) {
			auto itv = std::find(_regObjs.begin(), _regObjs.end(), objInfo);
			if (itv != _regObjs.end()) {
				_regObjs.erase(itv);
			}
		}
	}

	// 释放对象实例和 ObjectInfo 结构体（不持锁）
	deleteObjInfo(objInfo);
	return PCX_SUCCESS;
}

/**
 * @brief 释放由对象数组。
 * @param[in] objArray 待释放的对象数组。
 * @return 操作结果错误码。
 */
ErrorCode ObjectManager::freeObjectArray(IObjectArray* objArray)
{
	// `IObjectArray` 已有虚析构，直接通过接口指针 delete 即可正确析构派生类
	if (objArray) {
		delete objArray;
		return PCX_SUCCESS;
	}
	return PCX_PARAM_NULLPTR;
}

/**
 * @brief 按名称获取对象元信息。
 * @param[in] objName 对象名称。
 * @return 找到返回 ObjectInfo 指针；未找到返回 nullptr。
 */
ObjectInfo* ObjectManager::getObjInfo(const char* objName)
{
	// 校验对象名称是否为空
	if (objName == nullptr || objName[0] == '\0') {
		WriteLog(PcxLogLevel::Debug, "getObjInfo: objName is nullptr or empty");
		return nullptr;
	}

	std::lock_guard<std::recursive_mutex> lk(_mutex);
	auto it = _regObjsMap.find(objName);
	if (it == _regObjsMap.end()) {
		WriteLog(PcxLogLevel::Trace, "Object [%s] not found", objName);
		return nullptr;
	}
	return it->second;
}

/**
 * @brief 按名称查找对象实例地址。
 * @param[in] objName 对象名称。
 * @return 找到返回对象实例地址；未找到返回 nullptr。
 */
void* ObjectManager::findObject(const char* objName)
{
	// 校验对象名称是否为空
	if (objName == nullptr || objName[0] == '\0') {
		WriteLog(PcxLogLevel::Debug, "findObject: objName is nullptr or empty");
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
 * @brief 按组件扩展信息批量过滤对象。
 * @param[in] key 扩展信息键名数组。
 * @param[in] value 扩展信息值数组。
 * @param[in] count 键值对数量。
 * @return 匹配的对象数组，调用方需负责释放。
 */
IObjectArray* ObjectManager::findObjectByInfo(const char* key[], const char* value[], int count)
{
	if (key == nullptr || value == nullptr || count <= 0) {
		WriteLog(PcxLogLevel::Debug, "key or value is nullptr");
		return nullptr;
	}

	ObjectArray* objArray = new (std::nothrow) ObjectArray();
	if (objArray == nullptr) {
		WriteLog(PcxLogLevel::Fatal, "findObjectByInfo: alloc ObjectArray failed");
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
 * @brief 查询对象是否处于已登记状态。
 * @details PCX 当前未实现自动引用计数；原 `ObjectInfo::counter` 字段恒为 1，
 *          语义已退化为「存在性判断」，故这里直接按命中与否返回 1/0。
 * @param[in] objName 对象名称。
 * @return 已注册返回 1；未注册或入参非法返回 0。
 */
int ObjectManager::getRefCount(const char* objName)
{
	if (objName == nullptr || objName[0] == '\0') {
		WriteLog(PcxLogLevel::Debug, "getRefCount: objName is nullptr or empty");
		return 0;
	}
	ObjectInfo* objInfo = getObjInfo(objName);
	return objInfo ? 1 : 0;
}

/**
 * @brief 获取当前所有已注册对象列表的快照副本。
 * @return 成功返回当前所有已注册对象列表。
 */
std::vector<ObjectInfo*> ObjectManager::getRegisterObjects()
{
	std::lock_guard<std::recursive_mutex> lk(_mutex);
	return _regObjs;
}

PCX_END_NAMESPACE