/**
 * @file asiocontext.h
 * @brief 命名 Asio 上下文（io_context + 后台线程）注册表
 * @details 网络接口的 asioContextName 参数对应此处注册的上下文；
 *          同名上下文复用，避免每个对象各自起线程。
 */
#pragma once

#include <boost/asio/io_context.hpp>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace pch {

class AsioContext
{
public:
	AsioContext();
	~AsioContext();

	boost::asio::io_context& io() { return _io; }

private:
	boost::asio::io_context _io;
	boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _work;
	std::thread _thread;
};

class AsioContextManager
{
public:
	static AsioContextManager& instance();

	/** @brief 按名称获取上下文；null/空名称使用 "default"；不存在时创建 */
	std::shared_ptr<AsioContext> get(const char* name);

private:
	std::mutex _mutex;
	std::map<std::string, std::shared_ptr<AsioContext>> _contexts;
};

}
