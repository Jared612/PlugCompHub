#include "pcxasiocontext.h"

namespace pcx {

PcxAsioContext::~PcxAsioContext()
{
	stop();
}

void PcxAsioContext::start(size_t threadCount)
{
	if (_running) {
		return;
	}

	if (threadCount == 0) {
		threadCount = 1;
	}

	_ioContext.restart();
	_workGuard.reset(new boost::asio::executor_work_guard<boost::asio::io_context::executor_type>(
		boost::asio::make_work_guard(_ioContext)));
	_running = true;
	_workers.reserve(threadCount);

	for (size_t i = 0; i < threadCount; ++i) {
		_workers.emplace_back([this]() { _ioContext.run(); });
	}
}

void PcxAsioContext::stop()
{
	if (!_running) {
		return;
	}

	_workGuard.reset();
	_ioContext.stop();

	for (auto& worker : _workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	_workers.clear();

	_running = false;
}

size_t PcxAsioContext::getThreadCount() const
{
	return _workers.size();
}

void* PcxAsioContext::getIoContext()
{
	return &_ioContext;
}

}
