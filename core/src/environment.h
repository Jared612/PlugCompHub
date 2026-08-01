/**
 * @file environment.h
 * @brief 内置 `Environment`：实现 `IEnvironment`，进程内键值缓存，未命中则读系统 `getenv`。
 * @details 实现见 `environment.cpp`；`set` 只改缓存、不改系统环境变量。
 */

#pragma once
#include "interface.h"
#include "internal.h"
#include <list>
#include <mutex>
#include <string>
#include <map>

PCH_BEGIN_NAMESPACE

/**
 * @class Environment
 * @brief 实现 `IEnvironment`；缓存与 `get`/`set` 语义见本文件头注释。
 */
class Environment : public IEnvironment
{
public:

	/**
	 * @brief 实现 `IEnvironment::get`，读取环境变量值。
	 * @param[in] key 环境变量名
	 * @param[out] errCode 可选错误码输出指针
	 * @return 命中缓存或系统环境变量时返回值指针，失败返回 nullptr
	 */
	const char* get(const char* key, ErrorCode* errCode = nullptr);

	/**
	 * @brief 实现 `IEnvironment::set`，写入环境变量值。
	 * @param[in] key 环境变量名
	 * @param[in] value 环境变量值
	 * @return 成功返回 PCH_SUCCESS，失败返回 PCH_NULLPTR
	 */
	ErrorCode set(const char* key, const char* value);

private:
	std::mutex _mutex;                                // 互斥锁，保护内部缓存表
	std::map<std::string, std::string> _environment;  // 环境变量缓存表（key-value）
};

PCH_END_NAMESPACE