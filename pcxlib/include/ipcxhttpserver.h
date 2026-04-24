/**
 * @file ipcxhttpserver.h
 * @brief HTTP 服务器组件对外抽象接口。
 * @details 提供路由注册与请求/响应抽象；底层基于 Boost.Beast 实现。
 */
#pragma once
#include <cstddef>
#include <functional>

namespace pcx {

/**
 * @brief HTTP 请求只读视图。
 */
class IHttpRequest
{
public:
	virtual ~IHttpRequest() = default;

	virtual const char* getMethod() const = 0;
	virtual const char* getPath() const = 0;
	virtual const char* getTarget() const = 0;
	virtual const char* getHeader(const char* name) const = 0;
	virtual const char* getBody() const = 0;
	virtual size_t getBodySize() const = 0;
};

/**
 * @brief HTTP 响应构建器。
 */
class IHttpResponse
{
public:
	virtual ~IHttpResponse() = default;

	virtual void setStatus(unsigned code) = 0;
	virtual void setHeader(const char* name, const char* value) = 0;
	virtual void setBody(const char* data, size_t size) = 0;
	virtual void setBody(const char* text) = 0;
};

using HttpRouteHandler = std::function<void(const IHttpRequest&, IHttpResponse&)>;

/**
 * @brief PCX HTTP 服务器抽象接口。
 */
class IPcxHttpServer
{
public:
	virtual ~IPcxHttpServer() = default;

	/**
	 * @brief 启动监听。
	 * @param address 绑定地址，如 "0.0.0.0"。
	 * @param port    端口号。
	 * @param asioContextName 已创建的 IPcxAsioContext 对象名称。
	 * @return 成功返回 true。
	 */
	virtual bool start(const char* address, unsigned short port, const char* asioContextName) = 0;

	virtual void stop() = 0;

	/**
	 * @brief 注册路由处理器（应在 start 之前调用）。
	 * @param method HTTP 方法，如 "GET"、"POST"。
	 * @param path   精确路径，如 "/api/users"。
	 */
	virtual void addRoute(const char* method, const char* path, HttpRouteHandler handler) = 0;

	virtual bool isRunning() const = 0;
};

} // namespace pcx
