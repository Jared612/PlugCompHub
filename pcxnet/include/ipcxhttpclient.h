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
	/**
	 * @brief 异步请求完成回调。
	 * @param status_code HTTP 状态码；本地/传输失败时可能为 0。
	 * @param response 响应体文本。
	 * @param err_msg 失败时的可读说明；成功时通常为空。
	 */
	using HttpCallback = std::function<void(int status_code, const std::string& response, const std::string& err_msg)>;

	virtual ~IPcxHttpClient() = default;

	/**
	 * @brief 发起异步 HTTP GET。
	 * @param asioContextName 已在对象管理器中 `start` 的 `IPcxAsioContext` 注册名。
	 * @param host 主机名或 IP。
	 * @param port 端口字符串（如 `"8080"`）。
	 * @param target 路径与 query，如 `"/api/v1"`。
	 * @param callback 任意线程回调；不要在回调内阻塞过久。
	 */
	virtual void get(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, HttpCallback callback) = 0;

	/**
	 * @brief 发起异步 HTTP POST。
	 * @param body 请求体字节序列（文本/JSON 等由调用方编码）。
	 * @note 其余参数同 `get`。
	 */
	virtual void post(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, const std::string& body, HttpCallback callback) = 0;
};

} // namespace pcx
