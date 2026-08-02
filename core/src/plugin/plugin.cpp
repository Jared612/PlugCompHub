#include "coreexport.h"
#ifdef __PCH_SYS_WINDOWS
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include "error.h"
#include "loggerManager.h"
#include "plugin.h"
#include <assert.h>
#include "objectManager.h"

PCH_BEGIN_NAMESPACE

/** @brief PCH 全局对象管理器实例指针（由内核维护） */
extern ObjectManager* _objManager;

/**
 * @brief 获取平台相关的动态库错误信息
 * @return 错误消息字符串；无错误时返回空字符串
 */
static std::string getErrorMessage()
{
#ifdef __PCH_SYS_WINDOWS
	// GetLastError() 获取最近的系统错误码
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0) {
		return "";
	}
	LPSTR messageBuffer = nullptr;
	// 从错误码生成错误文本（系统分配缓冲区）
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
								 NULL,
								 errorMessageID,
								 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
								 (LPSTR)&messageBuffer,
								 0,
								 NULL);
	// FormatMessageA 失败时缓冲区可能未分配，size 为 0；避免用空指针构造 std::string
	if (size == 0 || messageBuffer == nullptr) {
		return "";
	}
	// 从此缓冲区构造 std::string
	std::string message(messageBuffer, size);
	// 释放系统分配的内存
	LocalFree(messageBuffer);
	// 返回错误文本	
	return message;
#elif defined(__PCH_SYS_LINUX) || defined(__PCH_SYS_MACOS)
	// 返回动态库错误文本
	return dlerror();
#endif
}

/**
 * @brief 加载插件动态库并构造 Plugin 对象
 * @param[in] path 插件动态库路径
 * @return 成功返回 Plugin 指针；失败返回 nullptr
 */
Plugin* Plugin::load(const char* path)
{
	// 如果路径为空，返回失败
	if (path == nullptr) {
		WriteLog(LogLevel::Debug, "Library load failed, empty path");
		return nullptr;
	}
	void* handle = nullptr;
	// 加载动态库
#ifdef __PCH_SYS_WINDOWS
	handle = ::LoadLibraryA(path);
#elif defined(__PCH_SYS_LINUX) || defined(__PCH_SYS_MACOS)
	// RTLD_NOW：在 dlopen 时解析所有符号，尽早暴露插件中的未定义符号；失败立即在此处报告错误
	handle = ::dlopen(path, RTLD_NOW);
#endif
	// 如果动态库加载失败，返回失败
	if (handle == nullptr) {
		WriteLog(LogLevel::Error, "Load [%s] failed, %s", path, getErrorMessage().c_str());
		return nullptr;
	}

	// 创建插件对象
	Plugin* plug = new Plugin(handle, path);

	// 初始化插件 API
	plug->initPluginAPI();
	
	// 返回插件对象
	return plug;
}

/**
 * @brief 析构插件对象并释放动态库句柄
 */
Plugin::~Plugin()
{
	// 如果动态库句柄不为空，释放它
	if (_handle != nullptr) {
#ifdef __PCH_SYS_WINDOWS
		::FreeLibrary((HMODULE)_handle);
		// 释放动态库句柄
#elif defined(__PCH_SYS_LINUX) || defined(__PCH_SYS_MACOS)
		::dlclose(_handle);
		// 释放动态库句柄
#endif
	}
}

/**
 * @brief 获取插件名称
 * @return 插件名称；无元数据时返回空字符串
 */
const char* Plugin::getName()
{
	if (_info) {
		return _info->name;
	}
	return "";
}

/**
 * @brief 获取插件版本
 * @return 插件版本；无元数据时返回空字符串
 */
const char* Plugin::getVersion()
{
	if (_info) {
		return _info->version;
	}
	return "";
}

/**
 * @brief 获取插件描述
 * @return 插件描述；无元数据时返回空字符串
 */
const char* Plugin::getDescription()
{
	if (_info) {
		return _info->description;
	}
	return "";
}

/**
 * @brief 获取插件编译时的 PCH ABI 版本
 * @return ABI 版本号；无元数据时返回 0
 */
uint32_t Plugin::getAbiVersion()
{
	if (_info) {
		return _info->abiVersion;
	}
	return 0;
}

/**
 * @brief 获取插件路径
 * @return 插件路径字符串
 */
const char* Plugin::getPath()
{
	return _path.c_str();
}

/**
 * @brief 获取插件导出的组件表
 * @return 组件表指针；无元数据时返回 nullptr
 */
ComponentInfo** Plugin::getComponentInfos()
{
	if (_info) {
		return _info->componentInfo;
	}
	return nullptr;
}

/**
 * @brief 从动态库解析导出符号地址
 * @param[in] symbol 导出符号名
 * @return 成功返回符号地址；失败返回 nullptr
 */
void* Plugin::resolve(const char* symbol)
{
	// 如果动态库句柄为空，返回失败
	if (_handle == nullptr) {
		WriteLog(LogLevel::Error, "Find symbols failed, library not loaded");
		return nullptr;
	}
	// 查找符号地址
	void* found = nullptr;
#ifdef __PCH_SYS_WINDOWS
	found = (void*)(::GetProcAddress((HMODULE)_handle, symbol)); 
#else
	found = (void*)(::dlsym(_handle, symbol));
#endif
	// 检查符号地址是否为空 
	if (found == nullptr) {
		WriteLog(LogLevel::Warning, "Symbol[%s] not found.", symbol);
		return nullptr;
	}
	// 返回符号地址
	return found;
}

/**
 * @brief 使用动态库句柄构造插件对象
 * @param[in] handle 动态库句柄
 * @param[in] path 插件文件路径
 */
Plugin::Plugin(void* handle, const char* path):
	_info(nullptr), _initFunc(nullptr)
{
	// 保存动态库句柄和插件路径，供后续符号解析和日志定位使用
	_handle = handle;
	_path = path;

	// pluginfo：插件必须导出元数据函数，返回 PluginInfo*（名称/版本/组件表等）
	typedef PluginInfo* (*__PLUGINFO)();
	__PLUGINFO pluginfo = (__PLUGINFO)resolve("pluginfo");
	if (pluginfo == nullptr) {
		WriteLog(LogLevel::Error, "failed to find pluginfo at: %s", path);
		return;
	}

	// 获取并缓存插件信息
	_info = pluginfo();

	// pluginit：插件初始化入口，稍后由 initPluginAPI() 调用
	_initFunc = resolve("pluginit");
	if (_initFunc == nullptr) {
		WriteLog(LogLevel::Error, "failed to find pluginit at: %s", path);
	}
}

/**
 * @brief 调用插件初始化入口 pluginit
 * @return 调用成功返回 true；pluginit 函数未找到返回 false
 */
bool Plugin::initPluginAPI()
{
	// 没有 pluginit 导出则无法初始化
	if (!_initFunc)
		return false;

	// pluginit 实际签名（由 PCH_PLUGIN_INFO_END 展开）为 void pluginit(void*)。
	// 此处类型定义必须匹配，否则通过不兼容的函数指针类型调用是未定义行为。
	typedef void (*__PLUGINIT)(void* objManager);

	// 传入全局对象管理器指针，完成插件侧初始化
	((__PLUGINIT)_initFunc)(_objManager);
	return true;
}

/**
 * @brief 调用插件导出的 pluginexit（如果存在）
 * @details 兼容旧插件：若未导出 pluginexit，直接返回，不记录错误
 */
void Plugin::exitPluginAPI()
{
	typedef void (*__PLUGINEXIT)();
	void* exitFunc = nullptr;
#ifdef __PCH_SYS_WINDOWS
	exitFunc = (void*)(::GetProcAddress((HMODULE)_handle, "pluginexit"));
#else
	exitFunc = (void*)(::dlsym(_handle, "pluginexit"));
#endif
	if (exitFunc == nullptr) {
		return;
	}
	((__PLUGINEXIT)exitFunc)();
}

PCH_END_NAMESPACE
