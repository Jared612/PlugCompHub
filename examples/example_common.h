/**
 * @file example_common.h
 * @brief 示例共用工具：定位可执行文件目录、拼接路径、按平台选择库文件名
 */
#pragma once

#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

static std::string exeDir()
{
#ifdef _WIN32
	char buf[MAX_PATH];
	DWORD n = GetModuleFileNameA(nullptr, buf, static_cast<DWORD>(sizeof(buf)));
	if (n == 0 || n >= sizeof(buf)) return ".";
	std::string p(buf, n);
	auto pos = p.find_last_of("\\/");
	return (pos == std::string::npos) ? "." : p.substr(0, pos);
#else
	char buf[PATH_MAX];
	ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (n <= 0) return ".";
	buf[n] = '\0';
	std::string p(buf);
	auto pos = p.find_last_of('/');
	return (pos == std::string::npos) ? "." : p.substr(0, pos);
#endif
}

static std::string joinPath(const std::string& d, const char* f)
{
#ifdef _WIN32
	return d + "\\" + f;
#else
	return d + "/" + f;
#endif
}

#ifdef _WIN32
#define PCH_CORE_LIB        "pch.dll"
#define PCH_THREADPOOL_LIB  "pchthreadpool.dll"
#define PCH_NETWORK_LIB     "pchnetwork.dll"
#define PCH_SQLITE_LIB      "pchsqlite.dll"
#define PCH_LOGGER_LIB      "pchlogger.dll"
#define PCH_APPLICATION_LIB "pchapplication.dll"
#elif defined(__APPLE__)
#define PCH_CORE_LIB        "libpch.dylib"
#define PCH_THREADPOOL_LIB  "libpchthreadpool.dylib"
#define PCH_NETWORK_LIB     "libpchnetwork.dylib"
#define PCH_SQLITE_LIB      "libpchsqlite.dylib"
#define PCH_LOGGER_LIB      "libpchlogger.dylib"
#define PCH_APPLICATION_LIB "libpchapplication.dylib"
#else
#define PCH_CORE_LIB        "libpch.so"
#define PCH_THREADPOOL_LIB  "libpchthreadpool.so"
#define PCH_NETWORK_LIB     "libpchnetwork.so"
#define PCH_SQLITE_LIB      "libpchsqlite.so"
#define PCH_LOGGER_LIB      "libpchlogger.so"
#define PCH_APPLICATION_LIB "libpchapplication.so"
#endif
