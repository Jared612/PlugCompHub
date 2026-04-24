/**
 * @file ipcxasiocontext.h
 * @brief Asio 事件循环组件对外抽象接口。
 * @details 管理 boost::asio::io_context 及其工作线程；HTTP / WebSocket 等网络组件共用同一实例。
 */
#pragma once
#include <cstddef>

namespace pcx {

/**
 * @brief PCX Asio 事件循环抽象接口。
 * @details 先 start 启动工作线程，再由 HTTP/WS 组件通过 getIoContext 获取底层 io_context 指针。
 */
class IPcxAsioContext
{
public:
	virtual ~IPcxAsioContext() = default;

	/**
	 * @brief 启动 io_context 工作线程。
	 * @param threadCount 线程数量；为 0 时按 1 处理。
	 */
	virtual void start(size_t threadCount = 1) = 0;

	/**
	 * @brief 停止事件循环并 join 所有工作线程。
	 */
	virtual void stop() = 0;

	/**
	 * @brief 当前工作线程数。
	 */
	virtual size_t getThreadCount() const = 0;

	/**
	 * @brief 获取底层 io_context 指针（实际类型为 boost::asio::io_context*）。
	 * @details 返回 void* 以避免在公共接口中暴露 Boost 头文件。
	 */
	virtual void* getIoContext() = 0;
};

} // namespace pcx
