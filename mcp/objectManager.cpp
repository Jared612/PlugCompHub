#include <string.h>
#include <algorithm>
#include <unordered_set>
#include <assert.h>
#include "error.h"
#include "loggerManager.h"
#include "objectManager.h"
#include "messagecenter.h"
#include "componentManager.h"

MCP_BEGIN_NAMESPACE

MCP_REGISTER_COMPONENT(mcp::ObjectManager, MCP_OBJECT_MANAGER_ID)

extern ComponentManager* _componentManager;  //全局组件管理器实例指针
extern MessageCenter* _msgCenter;            //全局消息中心实例指针

/**
 * @brief 释放对象信息及其关联的对象实例。
 * @param[in] objInfo 对象信息指针。外部调用删除后应按约定不要再使用，或自己置 nullptr
 */
static void deleteObjInfo(ObjectInfo* objInfo)
{
	// 校验对象信息及组件封装指针有效性
	if (objInfo && objInfo->component) {
		if (objInfo->object != nullptr && objInfo->component->getComponentInfo()->deletor != nullptr) {
			try {
				// 调用组件注册的析构函数销毁对象实例
				objInfo->component->getComponentInfo()->deletor(objInfo->object);
				// 释放对象实例内存
				objInfo->object = nullptr;
				// 释放 ObjectInfo 结构体自身
				if (objInfo!= nullptr) {
					delete objInfo;
					objInfo = nullptr;
                }
			} catch (...) {
				WriteLog(McpLogLevel::Debug, "Delete object [%s] error!", objInfo->objName.c_str());
			}
		}
	}
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
			*errCode = MCP_PARAM_INVALID;
		}
		WriteLog(McpLogLevel::Fatal, "Component is nullptr");
		return nullptr;
	}

	// 获取组件信息结构体指针，校验组件信息结构体指针是否为空，组件创建函数和销毁函数是否为空
	ComponentInfo *compInfo = component->getComponentInfo();
	if (compInfo == nullptr || compInfo->creator == nullptr || compInfo->deletor == nullptr) {
		if (errCode) {
			*errCode = MCP_PARAM_INVALID;
		}
		WriteLog(McpLogLevel::Fatal, "ComponentInfo is nullptr or creator or deletor is nullptr");
		return nullptr;
	}

	// 使用 nothrow 分配 ObjectInfo 结构体，避免内存不足时抛异常
	ObjectInfo* objInfo = new (std::nothrow) ObjectInfo();
	if (objInfo == nullptr) {
		WriteLog(McpLogLevel::Fatal, "The pointer is nullptr when the object is created, the memory may be exhausted!!");
		if (errCode) {
			*errCode = MCP_PARAM_INVALID;
		}
		return nullptr;
	}

	// 关联组件封装指针
	objInfo->component = component;
	// 初始化引用计数
	objInfo->counter = 1;

	// 通过组件的 creator 工厂函数创建实际对象实例，校验对象实例是否为空
	void* obj = compInfo->creator();
	if (obj == nullptr) {
		WriteLog(McpLogLevel::Fatal, "The pointer is nullptr when the object is created, the memory may be exhausted!!");
		if (errCode) {
			*errCode = MCP_OUTOFMEM;
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
	if (objInfo != nullptr)
	{
		// 如果提供了初始化消息，通过消息中心向对象派发 SystemObjectInit 事件
		if (initMsg) {
			// 将初始化消息转换为 Message 结构体
			Message* msg = (Message*)initMsg;
			msg->_code = SystemObjectInit;
			//通过消息中心向对象派发 SystemObjectInit 事件
			ErrorCode err = _msgCenter->invokeHandler(objInfo, initMsg, nullptr);
			if (errCode != nullptr)
				*errCode = err;
		}
		//返回对象实例地址
		return objInfo->object;
	}
	return nullptr;
}

/**
 * @brief 创建具名对象（内部版本）。
 * @details
 * 执行流程：参数校验 -> 重名检测 -> 通过组件管理器查找组件 ->
 * 创建对象实例 -> 注册到具名映射表 -> 记录调用位置信息。
 */
ObjectInfo* ObjectManager::createNamedObject(const char* componentID, const char* objName, ErrorCode* errCode, const char* file, int line)
{
	// 参数校验：组件 ID 和对象名不能为空
	if (componentID == nullptr || strcmp(componentID, "") == 0 || objName == nullptr || strcmp(objName, "") == 0){
		WriteLog(McpLogLevel::Debug, "Parameter is nullptr when CreateObject");
		return nullptr;
	}

	// 检查同名对象是否已存在，避免重复创建
	auto it = std::find_if(_regObjsMap.begin(), _regObjsMap.end(), [=](auto pair) {
		return pair.first == objName;
	});

	if (it != _regObjsMap.end()) {
		if (errCode) {
			*errCode = MCP_OBJECT_ADYEXIST;
		}
		WriteLog(McpLogLevel::Warning, "object[%s] already created", objName);
		return nullptr;
	}

	if (_componentManager) {
		// 通过组件管理器查找组件，并创建对象实例
		ObjectInfo* objInfo(createObjectInfo(_componentManager->findComp(componentID), errCode));
		if (nullptr == objInfo) {
			WriteLog(McpLogLevel::Error, "objInfo is nullptr when CreateObject");
			return nullptr;
        }

		// 将对象注册到名称映射表和地址映射表
		void* pobj = registerObj(objInfo, objName);
		if (pobj == nullptr) {
			// 注册失败（通常是名称冲突），回滚已创建的对象
			deleteObjInfo(objInfo);
			if (errCode) {
				*errCode = MCP_FAILED;
			}
			return nullptr;
		}

		// 记录调用方的源码位置，便于调试追踪
		if (file != nullptr) {
			objInfo->file = file;
			objInfo->line = line;
		}
		return objInfo;
	} else {
		if (errCode) {
			*errCode = MCP_COMPONENT_NULLPTR;
		}
		WriteLog(McpLogLevel::Warning, "ComponentManager is nullptr!");
		return nullptr;
	}
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
	if (objInfo != nullptr)
	{
		// 如果提供了初始化消息，向对象派发 SystemObjectInit 事件
		if (initMsg) {
			Message* msg = (Message*)initMsg;
			msg->_code = SystemObjectInit;

			ErrorCode err = _msgCenter->invokeHandler(objInfo, initMsg, nullptr);
			if (errCode != nullptr)
				*errCode = err;
		}

		return objInfo->object;
	}

	return nullptr;
}

/**
 * @brief 创建匿名对象（内部版本）。
 * @details
 * 匿名对象不注册到名称映射表中，仅加入全局对象列表 _regObjs。
 * 执行流程：参数校验 -> 查找组件 -> 创建对象实例 -> 加入对象列表。
 */
ObjectInfo* ObjectManager::createObject(const char* componentID, ErrorCode* errCode, const char* file, int line)
{
	// 参数校验：组件 ID 不能为空
	if (componentID == nullptr || strcmp(componentID, "") == 0) {
		WriteLog(McpLogLevel::Debug, "ComponentID is nullptr or empty");
		return nullptr;
	}

	// 组件管理器必须已初始化
	if (_componentManager == nullptr) {
		WriteLog(McpLogLevel::Warning, "ComponentManager is nullptr!");
		return nullptr;
	}

	// 通过组件 ID 查找组件封装
	Component* compInfo = _componentManager->findComp(componentID);
	if (compInfo == nullptr) {
		WriteLog(McpLogLevel::Debug, "Component %s not exists!", componentID);
		return nullptr;
	}

	// 创建 ObjectInfo 和实际对象实例
	ObjectInfo* objInfo(createObjectInfo(compInfo, errCode));
	if (objInfo == nullptr) {
		WriteLog(McpLogLevel::Debug, "Create ObjectInfo failed!");
		return nullptr;
	}

	// 记录调用方的源码位置
	if (file != nullptr) {
		objInfo->file = file;
		objInfo->line = line;
	}

	if (errCode) {
		*errCode = MCP_SUCCESS;
	}

	// 加锁后将匿名对象加入全局对象列表
	std::unique_lock<std::recursive_mutex> lk(_regMutex);
	_regObjs.push_back(objInfo);

	return objInfo;
}

/**
 * @brief 将对象注册到具名映射表、地址映射表及全局对象列表中。
 * @details
 * 注册过程持有 _regNamedMutex 和 _regMutex 两把锁，
 * 先操作映射表再操作列表，确保索引一致性。
 */
void* ObjectManager::registerObj(ObjectInfo* objInfo, const char* objName)
{
	assert(objInfo);
	assert(objName);

	{
		objInfo->objName = objName;
		// 加锁后在名称映射表中查找
		std::unique_lock<std::recursive_mutex> lk(_regNamedMutex);
		// 再次检查名称是否已被占用（双重校验，防并发）
		auto it = std::find_if(_regObjsMap.begin(), _regObjsMap.end(), [=](auto pair) {
			return pair.first == objName;
		});
		if (it == _regObjsMap.end()) {
			// 同时写入名称映射和地址映射
			_regObjsMap[objName] = objInfo;
			_regObjAddrMap[objInfo->object] = objInfo;
			lk.unlock();
			{
				// 将对象加入全局对象列表
				std::lock_guard<std::recursive_mutex> lkobj(_regMutex);
				_regObjs.push_back(objInfo);
			}
		} else {
			WriteLog(McpLogLevel::Warning, "Object[%s] already created", objName);
			return nullptr;
		}
	}

	assert(objInfo->component != nullptr);
	assert(objInfo->component->getComponentInfo() != nullptr);
	assert(objInfo->component->getComponentInfo()->componentID != nullptr);
	WriteLog(McpLogLevel::Trace, "Create component [%s], object name: [%s] succeed!", objInfo->component->getComponentInfo()->componentID, objName);
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
	if (objName == nullptr || strcmp(objName, "") == 0) {
		WriteLog(McpLogLevel::Debug, "ObjName is nullptr");
		return MCP_PARAM_INVALID;
	}

	ObjectInfo* objInfo = nullptr;
	{
		// 加锁后从具名映射表和地址映射表中移除
		std::unique_lock<std::recursive_mutex> lk(_regNamedMutex);
		auto it = std::find_if(_regObjsMap.begin(), _regObjsMap.end(), [=](auto pair) {
			return pair.first == objName;
		});
		if (it == _regObjsMap.end()) {
			return MCP_OBJECT_NOTFOUND;
		}

		if (it->second == nullptr) {
			return MCP_OBJECT_NULLPTR;
		}

		objInfo = it->second;
		_regObjsMap.erase(it);

		// 同步移除地址映射
		auto addrit = _regObjAddrMap.find(objInfo->object);
		if (addrit != _regObjAddrMap.end())
			_regObjAddrMap.erase(addrit);
	}

	{
		// 从全局对象列表中移除（按地址匹配）
		std::lock_guard<std::recursive_mutex> lkv(_regMutex);
		auto itv = std::find_if(_regObjs.begin(), _regObjs.end(), [&](ObjectInfo* objPtr) {
			assert(objPtr);
			return objPtr->object == objInfo->object;
		});
		if (itv != _regObjs.end()) {
			_regObjs.erase(itv);
		}
	}

	WriteLog(McpLogLevel::Trace, "Object [%s] ... deleted!", objName);
	// 释放对象实例内存
	deleteObject(objInfo->object);

	return MCP_SUCCESS;
}

/**
 * @brief 通过对象实例地址反查对象名称。
 * @param[in] obj 对象实例地址。
 * @return 对象名称。
 */
const char* ObjectManager::getObjectName(void* obj)
{
	// 校验对象实例地址是否为空
	if (obj == nullptr) {
		WriteLog(McpLogLevel::Debug, "Obj is nullptr");
		return nullptr;
	}
	
	// 加锁后在地址映射表中查找对应的 ObjectInfo
	std::unique_lock<std::recursive_mutex> lk(_regNamedMutex);

	// 在地址映射表中查找对应的 ObjectInfo
	auto it = _regObjAddrMap.find(obj);
	if (it != _regObjAddrMap.end()){
		// 如果找到对应的 ObjectInfo，则返回对象名称
		return it->second->objName.c_str();
	}
	// 如果未找到对应的 ObjectInfo，则返回空字符串
	return "";
}

/**
 * @brief 框架退出时批量清理所有对象。
 * @details
 * 按注册的逆序销毁对象，跳过核心管理器对象（ObjectManager、PluginManager、ComponentManager）以避免提前销毁基础设施。
 * 最后清空所有映射表。
 */
void ObjectManager::tearDown()
{
	// 加锁后逆序遍历，跳过核心管理器对象，销毁所有业务对象
	{
		std::lock_guard<std::recursive_mutex> lk(_regMutex);
		// 逆序遍历，跳过核心管理器对象，销毁所有业务对象
		for (auto it = _regObjs.rbegin(); it != _regObjs.rend(); it++) {
			if (*it) {
				if ((*it)->objName != std::string(MCP_DEFAULT_OBJECTMANAGER) 
					&& (*it)->objName != std::string(MCP_DEFAULT_PLUGINMANAGER)
					&& (*it)->objName != std::string(MCP_DEFAULT_COMPONENTMANAGER)
					) {
					// 销毁业务对象
					deleteObjInfo((*it));
				}
			}
		}
		// 清空全局对象列表
		_regObjs.clear();
	}
	// 加锁后清空名称映射表和地址映射表
	{
		std::lock_guard<std::recursive_mutex> lk(_regNamedMutex);
		_regObjsMap.clear();
		_regObjAddrMap.clear();
	}
}

/**
 * @brief 按对象地址删除对象。
 * @param[in] obj 对象实例地址。
 * @param[in] file 调用方源码文件路径。
 * @param[in] line 调用方源码行号。
 * @return 操作结果错误码。
 */
ErrorCode ObjectManager::deleteObject(void* obj,const char* file, int line)
{
	// 校验对象实例地址是否为空
	if (obj == nullptr) {
		return MCP_PARAM_NULLPTR;
	}

	// 设置返回错误码
	ErrorCode ret = MCP_SUCCESS;

	// 在全局对象列表中按地址查找并移除
	ObjectInfo* objInfo = nullptr;
	{
		// 加锁后在全局对象列表中按地址查找并移除
		std::lock_guard<std::recursive_mutex> lk(_regMutex);
		auto it = std::find_if(_regObjs.begin(), _regObjs.end(), [&](ObjectInfo* objPtr) {
			if (objPtr) {
				return objPtr->object == obj;
			}
			else {
				WriteLog(McpLogLevel::Error, "objPtr is nullptr.");
				return false;
			}
		});
		// 如果找到目标对象，则将对象信息赋值给 objInfo 并从全局对象列表中移除
		if (it != _regObjs.end()) {
			objInfo = *it;
			_regObjs.erase(it);
		}
		else {
			ret = MCP_OBJECT_NOTFOUND;
		}
	}
	
	// 如果找到目标对象，则从具名映射表和地址映射表中同步移除
	if (objInfo) {
		// 加锁后从具名映射表和地址映射表中同步移除
		std::lock_guard<std::recursive_mutex> lk(_regNamedMutex);
		auto it = _regObjsMap.find(objInfo->objName);
		if (it != _regObjsMap.end())
			_regObjsMap.erase(it);
		// 在地址映射表中按地址查找并移除
		auto addrit = _regObjAddrMap.find(objInfo->object);
		if (addrit != _regObjAddrMap.end())
			_regObjAddrMap.erase(addrit);

		// 释放对象实例和 ObjectInfo 结构体
		deleteObjInfo(objInfo);
	}

	return ret;
}

/**
 * @brief 释放由对象数组。
 * @param[in] objArray 待释放的对象数组。
 * @return 操作结果错误码。
 */
ErrorCode ObjectManager::freeObjectArray(IObjectArray* objArray)
{
	// 校验对象数组指针是否为空
	if (objArray)
	{
		// 释放对象数组
		delete (ObjectArray*)objArray;
		return MCP_SUCCESS;	
	}
	else {
		return MCP_PARAM_NULLPTR;
	}
}

/**
 * @brief 按名称获取对象元信息。
 * @param[in] objName 对象名称。
 * @return 找到返回 ObjectInfo 指针；未找到返回 nullptr。
 */
ObjectInfo* ObjectManager::getObjInfo(const char* objName)
{
	// 校验对象名称是否为空
	if (objName == nullptr || strcmp(objName, "") == 0) {
		WriteLog(McpLogLevel::Debug, "ObjName is nullptr or empty");
		return nullptr;
	}

	// 加锁后在名称映射表中查找
	std::unique_lock<std::recursive_mutex> lk(_regNamedMutex);

	// 在名称映射表中查找目标对象
	auto it = std::find_if(_regObjsMap.begin(), _regObjsMap.end(), [=](auto pair) {
		return pair.first == std::string(objName);
	});
	// 如果未找到目标对象，则返回 nullptr
	if (it == _regObjsMap.end()) {
		WriteLog(McpLogLevel::Trace, "The object named %s is not found!", objName);
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
	if (objName == nullptr || strcmp(objName, "") == 0) {
		WriteLog(McpLogLevel::Debug, "ObjName is nullptr");
		return nullptr;
	}
	// 在名称映射表中查找目标对象	
	void* p = nullptr;
	{
		// 加锁后在名称映射表中查找目标对象
		std::lock_guard<std::recursive_mutex> lk(_regNamedMutex);
		// 在名称映射表中查找目标对象
		auto it = std::find_if(_regObjsMap.begin(), _regObjsMap.end(), [=](auto pair) {
			return pair.first == objName;
		});
		if (it != _regObjsMap.end()) {
			if (it->second) {
				p = it->second->object;
			}
		}
	}

	return p;
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
	// 校验扩展信息键名数组和值数组是否为空
	if (key == nullptr || value == nullptr || count <= 0) {
		WriteLog(McpLogLevel::Debug, "key or value is nullptr");
		return nullptr;
	}

	// 创建对象数组
	ObjectArray* objArray = new ObjectArray();
	{
		// 加锁后遍历所有已注册对象
		std::lock_guard<std::recursive_mutex> lk(_regMutex);
		// 遍历所有已注册对象
		for (int i = 0 ; i < _regObjs.size() ; i++)
		{
			// 逐一比较所有键值对，全部匹配才算命中
			bool isMatch = true;
			for (int i = 0 ; i < count ; i++)
			{
				// 逐一比较所有键值对，如果不匹配，则将 isMatch 设置为 false 并退出循环
				if (strcmp(_regObjs[i]->component->getInfo(key[i]), value[i]) != 0)
				{
					isMatch = false;
					break;
				}
			}
			// 如果全部匹配，则将对象实例地址加入对象数组
			if (isMatch)
			{
				objArray->_objects.push_back((_regObjs[i])->object);
			}
		}
	}
	return objArray;
}

/**
 * @brief 获取具名对象的引用计数。
 * @param[in] objName 对象名称。
 * @return 引用计数值；对象不存在时返回 0。
 */
int ObjectManager::getRefCount(const char* objName)
{
	// 校验对象名称是否为空
	if (objName == nullptr || strcmp(objName, "") == 0) {
		WriteLog(McpLogLevel::Debug, "ObjName is nullptr or empty");
		return 0;
	}
	// 通过对象名称获取对象信息
	ObjectInfo* objInfo = getObjInfo(objName);
	// 对象存在时返回引用计数
	if (objInfo) {
		return objInfo->counter;
	}
	// 对象不存在时返回 0
	return 0;
}

/**
 * @brief 获取当前所有已注册对象列表的快照副本。
 * @return 成功返回当前所有已注册对象列表。
 */
std::vector<ObjectInfo*> ObjectManager::getRegisterObjects()
{
	return _regObjs;
}

MCP_END_NAMESPACE