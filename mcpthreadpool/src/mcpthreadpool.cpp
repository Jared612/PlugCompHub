#include "mcpthreadpool.h"
#include <chrono>

namespace mcp {

McpThreadPool::~McpThreadPool()
{
	stop();
}

void McpThreadPool::start(size_t number)
{	
	if (_started) {
		return;
	}

	_stopping = false;
	_started = true;

	if (number == 0) {
		number = 1;
	}

	_workers.reserve(number);
	for (size_t i = 0; i < number; ++i) {
		_workers.emplace_back([this]() { workerLoop(); });
	}
}

void McpThreadPool::exec(std::function<void()>&& task)
{
	if (!_started.load()) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(_mutex);
		_tasks.emplace(std::move(task));
	}

	_cv.notify_one();
}

void McpThreadPool::workerLoop()
{
	while (true) {
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_cv.wait(lock, [this]() {
				return _stopping || !_tasks.empty();
			});

			if (_stopping && _tasks.empty()) {
				return;
			}

			task = std::move(_tasks.front());
			_tasks.pop();
			if (task) {
				_runningTaskCount++;
			}
		}

		if (task) {
			task();
			_runningTaskCount--;
		}

		if (_tasks.empty()) {
			_cv.notify_all();
		}
	}
}

bool McpThreadPool::waitForAllDone(int millsecond)
{
	if (!_started) {
		return true;
	}

	std::unique_lock<std::mutex> lock(_mutex);
	if (millsecond < 0) {
		_cv.wait(lock, [this]() {
			return _tasks.empty() && _runningTaskCount.load() == 0;
		});
		return true;
	}

	auto timeout = std::chrono::milliseconds(millsecond);
	return _cv.wait_for(lock, timeout, [this]() {
		return _tasks.empty() && _runningTaskCount.load() == 0;
	});
}

size_t McpThreadPool::getThreadCount()
{
	return _workers.size();
}

size_t McpThreadPool::getTaskCount()
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _tasks.size();
}

void McpThreadPool::stop()
{
	if (!_started) {
		return;
	}

	_stopping = true;
	_cv.notify_all();

	for (auto& worker : _workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}

	_workers.clear();
	_started = false;
}

}
