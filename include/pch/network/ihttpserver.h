/**
 * @file ihttpserver.h
 * @brief HTTP 服务端抽象接口（基于 Boost.Beast 异步实现）
 */
#pragma once

#include <functional>
#include <string>

namespace pch {

class IHttpServer
{
public:
	/**
	 * @brief 请求处理器：根据方法/路径/请求体将响应写入调用方提供的缓冲区
	 * @param method 请求方法（如 "GET"）
	 * @param target 请求路径（如 "/ping"）
	 * @param body 请求体
	 * @param[out] out 响应缓冲区（由插件提供，容量 >= 4096）
	 * @param outSize 缓冲区容量（字节）
	 * @return 写入 out 的字节数；返回 <= 0 时按 500 处理
	 * @note 跨模块边界不返回持有堆内存的对象（std::string 等），
	 *       避免静态 CRT 下跨堆分配/释放导致死锁或崩溃
	 */
	using RequestHandler = std::function<int(const std::string& method, const std::string& target, const std::string& body, char* out, int outSize)>;

	virtual ~IHttpServer() = default;

	/**
	 * @brief 启动 HTTP 服务
	 * @param asioContextName 命名 Asio 上下文（不存在时自动创建）
	 * @param host 绑定地址（如 "127.0.0.1" / "0.0.0.0"）
	 * @param port 监听端口（如 "8080"）
	 * @param handler 请求处理器
	 * @return 成功返回 true；参数无效或监听失败返回 false
	 */
	virtual bool start(const char* asioContextName, const std::string& host, const std::string& port, RequestHandler handler) = 0;

	/** @brief 停止监听（已建立的连接由客户端关闭后回收） */
	virtual void stop() = 0;
};

}
