#pragma once

#include "imcpthreadpool.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace mcp {

class McpThreadPool : public IMcpThreadPool
{
public:
	McpThreadPool() = default;
	~McpThreadPool() override;

	void start(size_t number = 1) override;
	void exec(std::function<void()>&& task) override;
	bool waitForAllDone(int millsecond = -1) override;
	size_t getThreadCount() override;
	size_t getTaskCount() override;
	void stop() override;

private:
	void workerLoop();

	std::vector<std::thread> _workers;
	std::queue<std::function<void()>> _tasks;

	std::mutex _mutex;
	std::condition_variable _cv;

	std::atomic<size_t> _runningTaskCount{ 0 };
	std::atomic<bool> _started{ false };
	std::atomic<bool> _stopping{ false };
};

}
