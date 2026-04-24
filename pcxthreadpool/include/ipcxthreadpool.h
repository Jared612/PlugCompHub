/**
 * @file ipcxthreadpool.h
 * @brief PCX 线程池组件对外抽象：固定工作线程 + 任务队列。
 * @details 供业务将短任务异步投递；与消息中心异步队列解耦，由集成方选择是否组合使用。
 */
#pragma once

#include <functional>

namespace pcx {

/**
 * @brief 线程池：在若干工作线程上执行 `std::function<void()>` 任务。
 * @note `stop` 之后不应再 `exec`；`waitForAllDone` 用于优雅排空队列。
 */
class IPcxThreadPool
{
public:
	virtual ~IPcxThreadPool() = default;

	/**
	 * @brief 启动工作线程。
	 * @param number 线程数；小于 1 时实现可抬升到 1。
	 */
	virtual void start(size_t number = 1) = 0;

	/**
	 * @brief 投递异步任务（FIFO 顺序由实现保证）。
	 * @param task 可调用对象，右值移入。
	 * @note 未 `start` 时调用行为未定义，实现可忽略或入队至启动后执行。
	 */
	virtual void exec(std::function<void()>&& task) = 0;

	/**
	 * @brief 阻塞直到队列清空且运行中任务结束，或超时。
	 * @param millsecond 超时毫秒；`-1` 表示无限等待。
	 * @return 全部完成 true；超时或停止中未全部完成 false。
	 */
	virtual bool waitForAllDone(int millsecond = -1) = 0;

	/** @return 当前工作线程数。 */
	virtual size_t getThreadCount() = 0;

	/** @return 队列中待执行任务大致数量（估算即可）。 */
	virtual size_t getTaskCount() = 0;

	/** @brief 请求停止：停止接收新任务并 join 线程（语义以实现为准）。 */
	virtual void stop() = 0;
};

}
