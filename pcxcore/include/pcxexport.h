/**
 * @file pcxexport.h
 * @brief 本头提供：按平台区分编译选项、库导入导出、运行时按名字找函数、命名空间宏，以及 PCX 库对外导出的 C 风格入口声明。
 * @details 一般写业务时包含 `pcx.h` 即可；需要与库直接链接并调用导出函数时，会用到文末声明。
 */
#pragma once
#include <string.h>
#include <errno.h>

/******************************************************************************
 * 平台识别
 * - 根据编译器预定义宏识别当前操作系统
 * - 分别定义：__PCX_SYS_WINDOWS / __PCX_SYS_LINUX / __PCX_SYS_MACOS
 ******************************************************************************/
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)          // Windows 检测
	#define __PCX_SYS_WINDOWS                                        // PCX 的 Windows 平台标识
	// WIN32_LEAN_AND_MEAN - 减少 Windows.h 包含的内容，加快编译速度，避免引入不必要的 Windows API
	#ifndef WIN32_LEAN_AND_MEAN                                      
		#define WIN32_LEAN_AND_MEAN									 
	#endif
	// NOMINMAX - 避免 Windows.h 的 min/max 宏与 STL 的 std::min/std::max 冲突，避免在代码中使用 min/max 宏
	#ifndef NOMINMAX                                                 
		#define NOMINMAX
	#endif

#elif defined(linux) || defined(__linux)                            // Linux 检测
	#define __PCX_SYS_LINUX                                         // PCX Linux 平台标识
#elif defined(__APPLE__) || defined(MACOSX) || defined(macintosh) || defined(Macintosh) // macOS 检测
	#define __PCX_SYS_MACOS                                         // PCX macOS 平台标识
#else                                                               // 其他平台
	#error This operating system is not supported by this library   // 编译器报错：不支持的平台
#endif


/******************************************************************************
 * 导入/导出宏定义（跨平台）
 * - __PCX_API：用于类/函数声明，区分静态库、导出和导入
 * - __PCX_EXPORT：用于需要显式导出的符号
 * 编译MYCP.dll时: __MYCP_API ErrorCode Initialize(...) → __declspec(dllexport) ErrorCode Initialize(...)
 * 使用MYCP.dll时: __MYCP_API ErrorCode Initialize(...) → __declspec(dllimport) ErrorCode Initialize(...)
 ******************************************************************************/
#if defined(__PCX_SYS_WINDOWS)
	// Windows 平台下，按构建方式选择导入导出语义
#ifdef __PCX_STATIC
	#define __PCX_API
#elif defined(__PCX_EXPORTS)
	#define __PCX_API __declspec(dllexport)
#else
	#define __PCX_API __declspec(dllimport)
#endif
#else
	// Linux/macOS 通常不需要 dllimport/dllexport
	#define __PCX_API
#endif

// 始终导出符号 - 用于插件DLL的固定导出函数(如pluginfo、inquiry)，供主程序GetProcAddress查找
#if defined(__PCX_SYS_WINDOWS)
	#define __PCX_EXPORT __declspec(dllexport)
#else
	#define __PCX_EXPORT
#endif

/******************************************************************************
 * 平台相关头文件与辅助宏
 * - Windows：使用 LoadLibrary/GetProcAddress
 * - 非 Windows：使用 dlopen/dlsym
 ******************************************************************************/
#ifdef __PCX_SYS_WINDOWS
#include <Windows.h>
#pragma warning(disable:4003)
#else
#include <dlfcn.h>
#endif
#include <cstdio>

/******************************************************************************
 * 可变参数计数 / 宏拼接辅助（所有平台可用）
 * - VARGS(__VA_ARGS__) 产出 1~10 的参数个数；用于按参数个数派发到同族宏。
 * - CONCAT(a,b) 二级拼接，保证 a/b 先被展开再拼接。
 * - 以 `PCX_EXPAND(x) x` 包一层主要为 MSVC 的 `__VA_ARGS__` 展开顺序差异兜底。
 *****************************************************************************/
#define PCX_EXPAND(x) x
#define VARGS_(_10,_9,_8,_7,_6,_5,_4,_3,_2,_1,N,...) N
#define VARGS(...) PCX_EXPAND(VARGS_(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1,0))
#define CONCAT_(a,b) a##b
#define CONCAT(a,b) CONCAT_(a,b)


/**
 * @brief 获取 PCX 动态库句柄的存储引用。
 * @details `__linkto`/`__unlinkfrom` 共用同一处存储，避免重复加载与无处释放。
 *          作为 `static inline`，同一 TU 内的调用共享这份句柄；PCX 约定宿主在
 *          同一 TU（通常是调用 `pcx::api::Initialize/Terminate` 的文件）内成对使用。
 */
static inline void*& __pcxLibHandleRef()
{
	static void* handle = nullptr;
	return handle;
}

/******************************************************************************
* @brief 动态加载DLL并获取函数地址，应用程序不需要在编译时链接 pcx.dll ，而是在运行时动态加载，实现了解耦
* @param path    - DLL文件路径（如"pcx.dll"）
* @param symbol  - 要查找的函数符号名（如"Initialize"）
* @retval  nullptr - 加载失败或函数不存在
*          函数指针 - 查找成功，返回函数地址
*****************************************************************************/
static inline void *__linkto(const char *path, const char *symbol)
{
	// 检查参数是否为空
	if (symbol == nullptr)
		return nullptr;

	// 动态库句柄（单句柄缓存，由 __pcxLibHandleRef 提供，供 __unlinkfrom 成对释放）
	void*& handle = __pcxLibHandleRef();
	if (handle == nullptr) {
#ifdef __PCX_SYS_WINDOWS
		handle = ::LoadLibraryA(path);
#else
		handle = ::dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
#endif
		if (handle == nullptr) {
			fprintf(stderr, "Library [%s] load failed\n", path);
			return nullptr;
		}
	}

	// 解析符号地址，使用GetProcAddress获取函数地址
	void *found = nullptr;
#ifdef __PCX_SYS_WINDOWS
	found = (void *)(::GetProcAddress((HMODULE)handle, symbol));
#else
	found = (void *)(::dlsym(handle, symbol));
#endif
	return found;
}

/******************************************************************************
* @brief 释放由 `__linkto` 加载的动态库句柄。
* @details 修复 `__linkto` 把 handle 悬挂在进程生命周期内的资源泄漏；
*          `pcx::api::Terminate` 应在卸载完 PCX 后调用本函数。
*          多次调用安全：句柄为空时直接返回。
*****************************************************************************/
static inline void __unlinkfrom()
{
	void*& handle = __pcxLibHandleRef();
	if (handle == nullptr)
		return;
#ifdef __PCX_SYS_WINDOWS
	::FreeLibrary((HMODULE)handle);
#else
	::dlclose(handle);
#endif
	handle = nullptr;
}


// 命名空间辅助宏
#define PCX_BEGIN_NAMESPACE namespace pcx {
#define PCX_END_NAMESPACE }

// 默认动态库文件名（按平台区分）
#ifdef __PCX_SYS_WINDOWS
#define PCX_LIB_NAME "./pcx.dll"
#elif defined(__PCX_SYS_LINUX)
#define PCX_LIB_NAME "./libpcx.so"
#elif defined(__PCX_SYS_MACOS)
#define PCX_LIB_NAME "./libpcx.dylib"
#endif

// 使用 extern "C"：导出名为 Initialize、Terminate 等固定符号名，无 C++ 名字改编，便于外部链接；与 pcx.cpp 中定义一致。返回/错误码指针用 int，与 ErrorCode 同 ABI。
PCX_BEGIN_NAMESPACE

class IObjectManager;
class IMessage;

// 初始化 PCX，经 objectManager 传出对象管理器。
extern "C" __PCX_API int Initialize(IObjectManager*& objectManager);

// 创建匿名组件实例。
extern "C" __PCX_API void* CreateObject(const char* componentID, IMessage* initMsg, int* errCode = nullptr, const char* file = nullptr, int line = 0);

// 创建具名全局对象。
extern "C" __PCX_API void* CreateNamedObject(const char* componentID, const char* name, IMessage* initMsg, int* errCode = nullptr, const char* file = nullptr, int line = 0);

// 按名称查找对象。
extern "C" __PCX_API void* FindObject(const char* objName);

// 按指针删除对象。
extern "C" __PCX_API int DeleteObject(void* obj, const char* file, int line);

// 终止 PCX。
extern "C" __PCX_API int Terminate();

PCX_END_NAMESPACE