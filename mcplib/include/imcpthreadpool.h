#pragma once

#include <functional>

namespace mcp {

/** MCP 线程池抽象接口 */
class IMcpThreadPool
{
public:
	virtual ~IMcpThreadPool() = default;

	virtual void start(size_t number = 1) = 0;
	virtual void exec(std::function<void()>&& task) = 0;
	virtual bool waitForAllDone(int millsecond = -1) = 0;
	virtual size_t getThreadCount() = 0;
	virtual size_t getTaskCount() = 0;
	virtual void stop() = 0;
};

}
