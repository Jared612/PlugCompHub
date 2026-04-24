/**
 * @file environment.cpp
 * @brief `Environment` 实现：`get`/`set` 进程内缓存，未命中时 `get` 回退 `getenv`。
 * @details 声明与语义见 `environment.h`。
 */
#include "environment.h"
#include "error.h"

PCX_REGISTER_COMPONENT(pcx::Environment, PCX_ENVIROMENT_ID)

PCX_BEGIN_NAMESPACE

/**
 * @brief 实现 `IEnvironment::get`，读取环境变量值。
 * @param[in] key 环境变量名
 * @param[out] errCode 可选错误码输出指针
 * @return 命中缓存或系统环境变量时返回值指针，失败返回 nullptr
 */
const char* Environment::get(const char* key, ErrorCode* errCode)
{
	// 检查环境变量名是否为空，如果为空，则返回 nullptr
	if (key == nullptr)
	{
		// 如果错误码输出指针不为空，则设置错误码为 PCX_NULLPTR
		if (errCode != nullptr){
			*errCode = PCX_NULLPTR;
		}
		return nullptr;
	}

	// 检查错误码输出指针是否为空，如果为空，则设置错误码为 PCX_SUCCESS
	if (errCode != nullptr){
		*errCode = PCX_SUCCESS;
	}


	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 从缓存表中查找环境变量	
	auto it = _environment.find(key);
	// 如果找到，则返回环境变量值
	if (it != _environment.end())
	{
		return it->second.c_str();	
	}
	
	// 如果未找到，则调用 getenv 读取系统环境变量
	return getenv(key);
}

/**
 * @brief 实现 `IEnvironment::set`，写入环境变量值。
 * @param[in] key 环境变量名
 * @param[in] value 环境变量值
 * @return 成功返回 PCX_SUCCESS，失败返回 PCX_NULLPTR
 */
ErrorCode Environment::set(const char* key, const char* value)
{
	// 检查环境变量名和值是否为空，如果为空，则返回 PCX_NULLPTR
	if (key == nullptr || value == nullptr)
	{
		return PCX_NULLPTR;
	}

	// 加锁，防止多线程同时访问
	std::lock_guard<std::mutex> lk(_mutex);

	// 将环境变量值写入缓存表
	_environment[key] = value;
	return PCX_SUCCESS;
}

PCX_END_NAMESPACE
