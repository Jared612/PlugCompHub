/**
 * @file ipcxwebsocketclient.h
 * @brief WebSocket 客户端抽象接口（长连接）。
 * @details 通过 IPcxAsioContext 驱动异步 I/O；connect 建立会话后可 send/close。
 */
#pragma once

#include <functional>
#include <string>

namespace pcx {

class IPcxWebSocketClient
{
public:
	using MessageCallback = std::function<void(const std::string& msg)>;
	using ConnectCallback = std::function<void(bool success, const std::string& err_msg)>;
	using CloseCallback   = std::function<void()>;

	virtual ~IPcxWebSocketClient() = default;

	/** @brief 设置收到一帧文本/二进制解码后的回调（语义以实现为准）。 */
	virtual void setMessageCallback(MessageCallback cb) = 0;
	/** @brief 设置对端关闭或本地会话结束时的回调。 */
	virtual void setCloseCallback(CloseCallback cb)   = 0;

	/**
	 * @brief 异步建立 WebSocket 会话。
	 * @param asioContextName 已 `start` 的 `IPcxAsioContext` 对象名。
	 * @param host 服务器主机。
	 * @param port 端口字符串。
	 * @param target 握手 HTTP 路径，如 `"/"` 或 `"/ws"`。
	 * @param cb 连接结果：`success==false` 时 `err_msg` 说明原因。
	 */
	virtual void connect(const char* asioContextName, const std::string& host, const std::string& port,
		const std::string& target, ConnectCallback cb) = 0;

	/**
	 * @brief 在已连接会话上发送一帧消息。
	 * @note 未连接成功时调用行为未定义。
	 */
	virtual void send(const std::string& msg) = 0;

	/** @brief 主动关闭会话。 */
	virtual void close()                      = 0;
};

} // namespace pcx
