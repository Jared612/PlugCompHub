#include "threadpool.h"
#include <chrono>

namespace pch {

ThreadPool::~ThreadPool()
{
	stop();
}

void ThreadPool::start(size_t number)
{
	// 创建工作线程；start 创建线程，exec 提交任务
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

void ThreadPool::exec(std::function<void()>&& task)
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

void ThreadPool::workerLoop()
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


		// 检查必须在锁下进行，以避免与 exec() 入队产生数据竞争
		// 当队列为空且没有任务在运行时，唤醒等待 waitForAllDone 的线程
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

bool ThreadPool::waitForAllDone(int millsecond)
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

size_t ThreadPool::getThreadCount()
{
	return _workers.size();
}

size_t ThreadPool::getTaskCount()
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _tasks.size();
}

void ThreadPool::stop()
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
