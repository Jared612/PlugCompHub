/**
 * @file iwebsocketserver.h
 * @brief WebSocket 服务端抽象接口（基于 Boost.Beast 异步实现）
 */
#pragma once

#include <functional>
#include <string>

namespace pch {

class IWebSocketServer
{
public:
	/**
	 * @brief 消息处理器：收到文本消息后可通过 reply 回调回复
	 * @note reply 可在处理器返回后异步调用；连接关闭后回复将被静默丢弃
	 */
	using MessageHandler = std::function<void(const std::string& msg, const std::function<void(const std::string&)>& reply)>;

	virtual ~IWebSocketServer() = default;

	/**
	 * @brief 启动 WebSocket 服务
	 * @param asioContextName 命名 Asio 上下文（不存在时自动创建）
	 * @param host 绑定地址（如 "127.0.0.1" / "0.0.0.0"）
	 * @param port 监听端口（如 "8081"）
	 * @param handler 消息处理器
	 * @return 成功返回 true；参数无效或监听失败返回 false
	 */
	virtual bool start(const char* asioContextName, const std::string& host, const std::string& port, MessageHandler handler) = 0;

	/** @brief 停止监听（已建立的连接由客户端关闭后回收） */
	virtual void stop() = 0;
};

}
