#include "pcxthreadpool.h"
#include <chrono>

namespace pcx {

PcxThreadPool::~PcxThreadPool()
{
	stop();
}

void PcxThreadPool::start(size_t number)
{	
	// 固定数量 worker 线程 + 条件变量唤醒；未 start 时 exec 直接丢弃任务
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

void PcxThreadPool::exec(std::function<void()>&& task)
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

void PcxThreadPool::workerLoop()
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

		// 判空必须在持锁下读取 `_tasks`，避免与 `exec()` 的入队发生 data race；
		// 当队列已空且没有运行中任务时，唤醒可能在 `waitForAllDone` 上等待的线程。
		bool shouldNotify = false;
		{
			std::lock_guard<std::mutex> lock(_mutex);
			shouldNotify = _tasks.empty() && (_runningTaskCount.load() == 0);
		}
		if (shouldNotify) {
			_cv.notify_all();
		}
	}
}

bool PcxThreadPool::waitForAllDone(int millsecond)
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

size_t PcxThreadPool::getThreadCount()
{
	return _workers.size();
}

size_t PcxThreadPool::getTaskCount()
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _tasks.size();
}

void PcxThreadPool::stop()
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
