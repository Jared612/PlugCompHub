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

	/** @return HTTP 方法字符串，如 `"GET"`。 */
	virtual const char* getMethod() const = 0;
	/** @return 路径部分（不含主机）。 */
	virtual const char* getPath() const = 0;
	/** @return 原始 target（可含 query）。 */
	virtual const char* getTarget() const = 0;
	/**
	 * @param name 头名称（大小写策略以实现为准）。
	 * @return 头值指针；不存在返回 nullptr。
	 */
	virtual const char* getHeader(const char* name) const = 0;
	/** @return 请求体指针；无 body 时可能为空串指针。 */
	virtual const char* getBody() const = 0;
	/** @return 请求体字节长度。 */
	virtual size_t getBodySize() const = 0;
};

/**
 * @brief HTTP 响应构建器。
 */
class IHttpResponse
{
public:
	virtual ~IHttpResponse() = default;

	/** @brief 设置 HTTP 状态码，如 200/404。 */
	virtual void setStatus(unsigned code) = 0;
	/** @brief 追加/设置响应头字段。 */
	virtual void setHeader(const char* name, const char* value) = 0;
	/** @brief 设置二进制响应体。 */
	virtual void setBody(const char* data, size_t size) = 0;
	/** @brief 以 NUL 结尾文本设置响应体。 */
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

	/** @brief 停止监听并关闭已有连接（语义以实现为准）。 */
	virtual void stop() = 0;

	/**
	 * @brief 注册路由处理器（应在 start 之前调用）。
	 * @param method HTTP 方法，如 "GET"、"POST"。
	 * @param path   精确路径，如 "/api/users"。
	 */
	virtual void addRoute(const char* method, const char* path, HttpRouteHandler handler) = 0;

	/** @return 是否处于监听中。 */
	virtual bool isRunning() const = 0;
};

} // namespace pcx
