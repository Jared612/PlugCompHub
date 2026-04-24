/**
 * @file pcxcomponent.h
 * @brief 组件元数据、工厂函数类型，以及注册组件用的模板与宏。
 * @details 描述单个组件如何创建、销毁并挂到消息体系；依赖 `pcxexport.h` 中的命名空间宏。
 */
#pragma once
#include <functional>
#include <string>
#include "pcxexport.h"

PCX_BEGIN_NAMESPACE

class IMessageHandler;                                                 //消息处理接口类，前置声明
using ConstructorFunc = std::function<void *()>;                       // 构造函数指针类型，返回值为void*，参数为空
using DestructorFunc = std::function<void(void *)>;                    // 析构函数指针类型，参数为void*
using ChangeTypeFunc = std::function<pcx::IMessageHandler* (void *)>;  // 类型转换函数指针类型，返回值为IMessageHandler*，参数为void*

/**
 * @brief 组件信息结构体
 * @details 组件信息结构体，包含组件的构造函数、析构函数、类型转换函数、组件名称和详细信息
 */
struct ComponentInfo
{
	ConstructorFunc creator;				// 构造函数指针，返回值为void*，参数为空	
	DestructorFunc deletor;					// 析构函数指针，参数为void*
	ChangeTypeFunc getMessageHandler;		// 类型转换函数指针，返回值为IMessageHandler*，参数为void*
	const char *componentID;				// 组件名称
};

/**
 * @brief 组件模板类
 * @details 为任意组件类型自动提供工厂方法，包括创建、销毁和类型转换功能
 * @tparam ComponentT 组件类型
 */
template<class ComponentT>
class ComponentTmpl
{
public:
	/**
	 * @brief 组件信息静态实例
	 * @details 包含该组件的创建函数、销毁函数、类型转换函数和组件ID
	 */
	static const ComponentInfo componentInfo;

public:
	/**
	 * @brief 创建组件实例
	 * @return void* 组件实例指针，创建失败返回 nullptr
	 * @details 使用 nothrow 版本的 new 分配内存，避免异常抛出
	 */
	static void *creator()
	{
		ComponentT *com = new(std::nothrow) ComponentT(); // 使用 nothrow 版本的 new 分配内存，避免异常抛出
		return com;
	}

	/**
	 * @brief 销毁组件实例
	 * @param objptr 组件实例指针
	 * @details 安全地删除组件实例，指针为空时不执行任何操作
	 */
	static void deletor(void *obj)
	{
		if (obj == nullptr)
			return;

		delete (ComponentT *)obj;
	}

	/**
	 * @brief 获取组件消息处理函数
	 * @param obj 组件实例指针（指向 `ComponentT` 实例）
	 * @return IMessageHandler* 消息处理函数指针，组件不继承 IMessageHandler 时返回 nullptr
	 * @details 类型关系在编译期用 `is_base_of` 判定，转换走 `static_cast`：
	 *          既避免 RTTI 依赖（允许 `-fno-rtti` / `/GR-`），也比 `dynamic_cast` 快。
	 */
	template<class U = ComponentT>
	static typename std::enable_if<std::is_base_of<IMessageHandler, U>::value, pcx::IMessageHandler*>::type
		getMessageHandlerImpl(void* obj)
	{
		return static_cast<IMessageHandler*>(static_cast<U*>(obj));
	}

	template<class U = ComponentT>
	static typename std::enable_if<!std::is_base_of<IMessageHandler, U>::value, pcx::IMessageHandler*>::type
		getMessageHandlerImpl(void* /*obj*/)
	{
		return nullptr;
	}

	static pcx::IMessageHandler* getMessageHandler(void *obj)
	{
		return getMessageHandlerImpl<ComponentT>(obj);
	}
};

/**
 * @brief 组件注册宏
 * @details 为指定组件类型创建并初始化 ComponentInfo 静态实例
 * @param CLASS 组件类名
 * @param ID 组件唯一标识符
 * @example
 * @code
 * // 在组件实现文件中使用
 * class MyComponent {
 * public:
 *     void doSomething() {
 *         // 组件功能实现
 *     }
 * };
 * 
 * // 注册组件
 * MYCP_COMPONENT_REGISTER(MyComponent, "myapp.component.mycomponent")
 * 
 * // 上面这行代码展开后：
 * // const mycp::ComponentInfo mycp::Component<MyComponent>::cinfo = {
 * //     &mycp::Component<MyComponent>::creator,
 * //     &mycp::Component<MyComponent>::deletor,
 * //     &mycp::Component<MyComponent>::ChangeType,
 * //     "myapp.component.mycomponent"
 * // };
 * @endcode
 */
// 组件注册宏
#define PCX_REGISTER_COMPONENT(CLASSNAME, ID) \
template<> const pcx::ComponentInfo pcx::ComponentTmpl<CLASSNAME>::componentInfo = { \
	&pcx::ComponentTmpl<CLASSNAME>::creator, \
	&pcx::ComponentTmpl<CLASSNAME>::deletor, \
	&pcx::ComponentTmpl<CLASSNAME>::getMessageHandler, \
	ID \
};

// 核心组件ID（统一命名）
#define PCX_COMPONENT_MANAGER_ID	"pcx.core.componentManager"
#define PCX_OBJECT_MANAGER_ID		"pcx.core.objectManager"
#define PCX_PLUGIN_MANAGER_ID		"pcx.core.pluginManager"
#define PCX_MESSAGECENTER_ID		"pcx.core.messageCenter"
#define PCX_LOGGERMANAGER_ID		"pcx.core.loggerManager"
#define PCX_ENVIROMENT_ID			"pcx.core.environment"

// 核心组件默认实例名（统一命名，严禁删除默认核心组件对象）
#define PCX_DEFAULT_COMPONENTMANAGER	"pcx.core.componentManager.name"
#define PCX_DEFAULT_OBJECTMANAGER		"pcx.core.objectManager.name"
#define PCX_DEFAULT_PLUGINMANAGER		"pcx.core.pluginManager.name"
#define PCX_DEFAULT_MESSAGECENTER		"pcx.core.messageCenter.name"
#define PCX_DEFAULT_LOGGERMANAGER		"pcx.core.loggerManager.name"
#define PCX_DEFAULT_ENVIROMENT			"pcx.core.environment.name"

// 环境变量名（统一 core 命名风格）
#define PCX_COMMANDDISPATCHER_NAME		"pcx.core.commandDispatcher.name"
#define PCX_EVENTDISPATCHER_NAME		"pcx.core.eventDispatcher.name"

PCX_END_NAMESPACE
