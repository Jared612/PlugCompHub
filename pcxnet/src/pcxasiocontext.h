#pragma once

#include "ipcxasiocontext.h"

#include <boost/asio.hpp>

#include <atomic>
#include <thread>
#include <vector>

namespace pcx {

class PcxAsioContext : public IPcxAsioContext
{
public:
	PcxAsioContext() = default;
	~PcxAsioContext() override;

	void start(size_t threadCount = 1) override;
	void stop() override;
	size_t getThreadCount() const override;
	void* getIoContext() override;

private:
	boost::asio::io_context _ioContext;
	std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> _workGuard;
	std::vector<std::thread> _workers;

	std::atomic<bool> _running{ false };
};

}
