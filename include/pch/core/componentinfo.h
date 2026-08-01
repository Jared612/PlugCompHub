/**
 * @file componentinfo.h
 * @brief 组件元数据、工厂函数类型以及组件注册的模板和宏
 * @details 描述单个组件如何创建、销毁和挂接到消息系统；依赖 coreexport.h 中的命名空间宏
 */
#pragma once
#include <functional>
#include <string>
#include "coreexport.h"

PCH_BEGIN_NAMESPACE

class IMessageHandler;                                                 // 消息处理器接口，前向声明
using ConstructorFunc = std::function<void *()>;                       // 构造函数：返回 void*，无参数
using DestructorFunc = std::function<void(void *)>;                    // 析构函数：接受 void*
using ChangeTypeFunc = std::function<pch::IMessageHandler* (void *)>;  // 类型转换函数指针，返回 IMessageHandler*，接受 void*

/**
 * @brief 组件信息结构体
 * @details 包含组件的构造、析构、类型转换函数、组件名称和详细信息
 */
struct ComponentInfo
{
	ConstructorFunc creator;				// 构造函数
	DestructorFunc deletor;					// 析构函数
	ChangeTypeFunc getMessageHandler;		// 类型转换函数指针，返回 IMessageHandler*，接受 void*
	const char *componentID;				// 组件名称
};

/**
 * @brief 组件模板类
 * @details 为任意组件类型自动提供工厂方法，包括创建、销毁和类型转换
 * @tparam ComponentT 组件类型
 */
template<class ComponentT>
class ComponentTmpl
{
public:
	/**
	 * @brief 组件信息静态实例
	 * @details 包含此组件的创建函数、销毁函数、类型转换函数和组件 ID
	 */
	static const ComponentInfo componentInfo;

public:
	/**
	 * @brief 创建组件实例
	 * @return void* 组件实例指针，失败返回 nullptr
	 * @details 使用 nothrow 版本的 new 分配内存，避免异常
	 */
	static void *creator()
	{
		ComponentT *com = new(std::nothrow) ComponentT(); // 使用 nothrow new 分配，避免异常
		return com;
	}

	/**
	 * @brief 销毁组件实例
	 * @param objptr 组件实例指针
	 * @details 安全删除组件实例；指针为 null 时无操作
	 */
	static void deletor(void *obj)
	{
		if (obj == nullptr)
			return;

		delete (ComponentT *)obj;
	}

	/**
	 * @brief 获取组件消息处理器
	 * @param obj 组件实例指针（指向 ComponentT 实例）
	 * @return IMessageHandler* 消息处理器指针，若组件未继承 IMessageHandler 则返回 nullptr
	 * @details 类型关系在编译时通过 is_base_of 确定，转换使用 static_cast。
	 *          避免 RTTI 依赖（允许 -fno-rtti / /GR-），也比 dynamic_cast 更快。
	 */
	template<class U = ComponentT>
	static typename std::enable_if<std::is_base_of<IMessageHandler, U>::value, pch::IMessageHandler*>::type
		getMessageHandlerImpl(void* obj)
	{
		return static_cast<IMessageHandler*>(static_cast<U*>(obj));
	}

	template<class U = ComponentT>
	static typename std::enable_if<!std::is_base_of<IMessageHandler, U>::value, pch::IMessageHandler*>::type
		getMessageHandlerImpl(void* /*obj*/)
	{
		return nullptr;
	}

	static pch::IMessageHandler* getMessageHandler(void *obj)
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
 *         // 组件功能
 *     }
 * };
 * 
 * // 注册组件
 * MYCP_COMPONENT_REGISTER(MyComponent, "myapp.component.mycomponent")
 * 
 * // 上述展开为：
 * // const mycp::ComponentInfo mycp::Component<MyComponent>::cinfo = {
 * //     &mycp::Component<MyComponent>::creator,
 * //     &mycp::Component<MyComponent>::deletor,
 * //     &mycp::Component<MyComponent>::ChangeType,
 * //     "myapp.component.mycomponent"
 * // };
 * @endcode
 */
#define PCH_REGISTER_COMPONENT(CLASSNAME, ID) \
template<> const pch::ComponentInfo pch::ComponentTmpl<CLASSNAME>::componentInfo = { \
	&pch::ComponentTmpl<CLASSNAME>::creator, \
	&pch::ComponentTmpl<CLASSNAME>::deletor, \
	&pch::ComponentTmpl<CLASSNAME>::getMessageHandler, \
	ID \
};

#define PCH_COMPONENT_MANAGER_ID "PCH.core.componentManager"
#define PCH_OBJECT_MANAGER_ID		"PCH.core.objectManager"
#define PCH_PLUGIN_MANAGER_ID		"PCH.core.pluginManager"
#define PCH_MESSAGECENTER_ID		"PCH.core.messageCenter"
#define PCH_LOGGERMANAGER_ID		"PCH.core.loggerManager"
#define PCH_ENVIROMENT_ID			"PCH.core.environment"

// 核心组件默认实例名称（统一命名；请勿删除默认核心组件对象）
#define PCH_DEFAULT_COMPONENTMANAGER	"PCH.core.componentManager.name"
#define PCH_DEFAULT_OBJECTMANAGER		"PCH.core.objectManager.name"
#define PCH_DEFAULT_PLUGINMANAGER		"PCH.core.pluginManager.name"
#define PCH_DEFAULT_MESSAGECENTER		"PCH.core.messageCenter.name"
#define PCH_DEFAULT_LOGGERMANAGER		"PCH.core.loggerManager.name"
#define PCH_DEFAULT_ENVIROMENT			"PCH.core.environment.name"

// 环境变量名称（统一核心命名风格）
#define PCH_COMMANDDISPATCHER_NAME		"PCH.core.commandDispatcher.name"
#define PCH_EVENTDISPATCHER_NAME		"PCH.core.eventDispatcher.name"

PCH_END_NAMESPACE
