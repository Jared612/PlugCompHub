/**
 * @file coreexport.h
 * @brief 平台感知的编译选项、库导入/导出、动态加载及 C 风格入口点
 * @details 一般用途包含 "core.h"；末尾的声明用于直接链接库
 */
#pragma once
#include <string.h>
#include <errno.h>

/******************************************************************************
 * 平台检测
 * - 通过编译器预定义宏检测当前操作系统
 * - 定义：__PCH_SYS_WINDOWS / __PCH_SYS_LINUX / __PCH_SYS_MACOS
 ******************************************************************************/
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
	#define __PCH_SYS_WINDOWS

	// WIN32_LEAN_AND_MEAN 减少 Windows.h 包含量，加速编译
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif

	// NOMINMAX 防止 Windows.h 的 min/max 宏与 STL 冲突
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif

#elif defined(linux) || defined(__linux)
	#define __PCH_SYS_LINUX

#elif defined(__APPLE__) || defined(MACOSX) || defined(macintosh) || defined(Macintosh)
	#define __PCH_SYS_MACOS

#else
	#error 此操作系统不受此库支持
#endif


/******************************************************************************
 * 导入/导出宏（跨平台）
 * - __PCH_API：用于类/函数声明；区分静态库、导出、导入
 * - __PCH_EXPORT：用于需要显式导出的符号
 ******************************************************************************/
#if defined(__PCH_SYS_WINDOWS)
	#ifdef __PCH_STATIC
		#define __PCH_API
	#elif defined(__PCH_EXPORTS)
		#define __PCH_API __declspec(dllexport)
	#else
		#define __PCH_API __declspec(dllimport)
	#endif
#else
	#define __PCH_API
#endif

// 始终导出的符号：用于固定的 DLL 导出函数（pluginfo, inquiry）
// 主机程序通过 GetProcAddress/dlvsym 使用
#if defined(__PCH_SYS_WINDOWS)
	#define __PCH_EXPORT __declspec(dllexport)
#else
	#define __PCH_EXPORT
#endif


/******************************************************************************
 * 平台相关的头文件和辅助函数
 * - Windows：LoadLibrary/GetProcAddress
 * - 非 Windows：dlopen/dlsym
 ******************************************************************************/
#ifdef __PCH_SYS_WINDOWS
	#include <Windows.h>
	#pragma warning(disable:4003)
#else
	#include <dlfcn.h>
#endif
#include <cstdio>

/******************************************************************************
 * PCH ABI 版本（major.minor）
 * - 公共接口结构/虚表布局一旦变更（如 PluginInfo 增删字段、接口类增删虚函数），
 *   必须递增此版本；core 加载插件时校验，不匹配则拒绝加载。
 *****************************************************************************/
#define PCH_ABI_VERSION 0x0201u


/******************************************************************************
 * 变参计数 / 宏拼接辅助
 * - VARGS(__VA_ARGS__) 生成 1..10 的参数计数；分派到同族宏
 * - CONCAT(a,b) 两级拼接，确保 a/b 先展开
 * - PCH_EXPAND(x) x：用于绕过 MSVC 在 __VA_ARGS__ 展开上的差异
 *****************************************************************************/
#define PCH_EXPAND(x) x
#define VARGS_(_10,_9,_8,_7,_6,_5,_4,_3,_2,_1,N,...) N
#define VARGS(...) PCH_EXPAND(VARGS_(__VA_ARGS__,10,9,8,7,6,5,4,3,2,1,0))
#define CONCAT_(a,b) a##b
#define CONCAT(a,b) CONCAT_(a,b)


/**
 * @brief 获取 PCH 动态库句柄的存储引用
 * @details __linkto / __unlinkfrom 共享一个句柄存储，
 *          避免重复加载和未释放的资源。
 *          作为静态内联，同一翻译单元内的所有调用共享此句柄。
 *          PCH 约定：主机应在同一翻译单元中配对使用 __linkto / __unlinkfrom
 */
static inline void*& __PCHLibHandleRef()
{
	static void* handle = nullptr;
	return handle;
}


/******************************************************************************
 * @brief 动态加载 DLL 并获取函数地址。
 *        应用程序无需在编译时链接 PCH.dll；
 *        而是在运行时动态加载以实现解耦。
 * @param path   - DLL 文件路径（如 "PCH.dll"）
 * @param symbol - 函数符号名（如 "Initialize"）
 * @retval nullptr - 加载失败或函数未找到
 *         函数指针 - 成功
 *****************************************************************************/
static inline void *__linkto(const char *path, const char *symbol)
{
	if (symbol == nullptr)
		return nullptr;

	void*& handle = __PCHLibHandleRef();
	if (handle == nullptr) {
#ifdef __PCH_SYS_WINDOWS
		handle = ::LoadLibraryA(path);
#else
		handle = ::dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
#endif
		if (handle == nullptr) {
			fprintf(stderr, "Library [%s] load failed\n", path);
			return nullptr;
		}
	}

	void *found = nullptr;
#ifdef __PCH_SYS_WINDOWS
	found = (void *)(::GetProcAddress((HMODULE)handle, symbol));
#else
	found = (void *)(::dlsym(handle, symbol));
#endif
	return found;
}


/******************************************************************************
 * @brief 释放由 __linkto 加载的库句柄
 * @details 修复 __linkto 的句柄在进程生命周期内持续存在的资源泄漏问题。
 *          pch::api::Terminate 应在卸载 PCH 后调用此方法。
 *          可多次安全调用：句柄为 null 时无操作。
 *****************************************************************************/
static inline void __unlinkfrom()
{
	void*& handle = __PCHLibHandleRef();
	if (handle == nullptr)
		return;
#ifdef __PCH_SYS_WINDOWS
	::FreeLibrary((HMODULE)handle);
#else
	::dlclose(handle);
#endif
	handle = nullptr;
}


// 命名空间辅助
#define PCH_BEGIN_NAMESPACE namespace pch {
#define PCH_END_NAMESPACE }

// 默认库文件名（平台相关）
#ifdef __PCH_SYS_WINDOWS
	#define PCH_LIB_NAME "./pch.dll"
#elif defined(__PCH_SYS_LINUX)
	#define PCH_LIB_NAME "./libpch.so"
#elif defined(__PCH_SYS_MACOS)
	#define PCH_LIB_NAME "./libpch.dylib"
#endif

// extern "C"：导出固定符号名（Initialize, Terminate 等）
// 不使用 C++ 名字改编，方便外部链接。
// 定义需与 PCH.cpp 匹配。错误码以 int 返回
// 以匹配 ErrorCode 的 ABI。
PCH_BEGIN_NAMESPACE

class IObjectManager;
class IMessage;

// 初始化 PCH；对象管理器通过 objectManager 传出
extern "C" __PCH_API int Initialize(IObjectManager*& objectManager);

// 创建匿名组件实例
extern "C" __PCH_API void* CreateObject(const char* componentID, IMessage* initMsg, int* errCode = nullptr, const char* file = nullptr, int line = 0);

// 创建命名全局对象
extern "C" __PCH_API void* CreateNamedObject(const char* componentID, const char* name, IMessage* initMsg, int* errCode = nullptr, const char* file = nullptr, int line = 0);

// 按名称查找对象
extern "C" __PCH_API void* FindObject(const char* objName);

// 按指针删除对象
extern "C" __PCH_API int DeleteObject(void* obj, const char* file, int line);

// 终止 PCH
extern "C" __PCH_API int Terminate();

PCH_END_NAMESPACE
