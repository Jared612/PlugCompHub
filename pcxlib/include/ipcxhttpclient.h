/**
 * @file ipcxhttpclient.h
 * @brief 异步 HTTP 客户端抽象接口（单次请求 / 独立 Session）。
 * @details 与 IPcxHttpServer 相同，通过已注册的 IPcxAsioContext 对象名获取 io_context。
 */
#pragma once

#include <functional>
#include <string>

namespace pcx {

class IPcxHttpClient
{
public:
	/// 回调：HTTP 状态码、响应体、错误信息（失败时 status 可为 0）
	using HttpCallback = std::function<void(int status_code, const std::string& response, const std::string& err_msg)>;

	virtual ~IPcxHttpClient() = default;

	/**
	 * @param asioContextName 已 start 的 IPcxAsioContext 在对象管理器中的名称（与 IPcxHttpServer::start 一致）
	 */
	virtual void get(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, HttpCallback callback) = 0;

	virtual void post(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, const std::string& body, HttpCallback callback) = 0;
};

} // namespace pcx
