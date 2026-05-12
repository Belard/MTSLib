#include "scheduler.h"

namespace mtsLib
{
    scheduler::scheduler() : ImtsResource() {}
    scheduler::scheduler(int threadNumber) : ImtsResource(threadNumber) {}

    scheduler::~scheduler()
    {
        stop();
    }

    void scheduler::start()
    {
        if (!m_stopTaskExecutor.exchange(false))
        {
            throw std::runtime_error("Scheduler is already running.");
        }

        for (int i = 0; i < m_threadNumber; ++i)
        {
            m_threads.emplace_back(&scheduler::taskExecutor, this);
        }

    }

    void scheduler::stop()
    {
        m_stopTaskExecutor.store(true);
        m_taskExecutorCondition.notify_all();
        for (auto& t : m_threads)
            if (t.joinable()) t.join();
        m_threads.clear();
    }

    void scheduler::addTask(schedulerTask&& task)
    {
        std::lock_guard<std::mutex> lock(m_taskQueueMutex);
        m_taskQueue.push(std::move(task));
    }

    void scheduler::taskExecutor()
    {
        while (true)
        {
            std::function<void()> task = getTask();
            if (!task)
            {
                if (m_stopTaskExecutor.load())
                    break;
                continue;
            } 
            else 
            {
                task();
            }
        }
    }

    std::function<void()> scheduler::getTask()
    {
        std::unique_lock<std::mutex> lock(m_taskQueueMutex);
        while (m_taskQueue.empty())
        {
            if (m_stopTaskExecutor.load())
                return nullptr;
            m_taskExecutorCondition.wait(lock);
        }

        auto now = std::chrono::steady_clock::now();
        while (!m_taskQueue.empty() && m_taskQueue.top().executeAt > now)
        {
            auto waitDuration = m_taskQueue.top().executeAt - now;
            m_taskExecutorCondition.wait_for(lock, waitDuration);
            now = std::chrono::steady_clock::now();
        }

        if (m_taskQueue.empty() || m_taskQueue.top().executeAt > now || m_stopTaskExecutor.load())
            return nullptr;

        auto task = std::move(m_taskQueue.top().taskFunction);
        m_taskQueue.pop();
        return task;
    }
    

}