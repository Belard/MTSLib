#include "task.h"

namespace mtsLib
{
    task::task() : ImtsResource() {}
    task::task(int threadNumber) : ImtsResource(threadNumber) {}

    task::~task()
    {
        stop();
    }

    void task::execute()
    {
        m_stopTaskExecutor.store(false);
        for (auto i = 0; i < m_threadNumber; i++)
        {
            m_threads.emplace_back(&task::taskExecutor, this);
        }
    }

    void task::stop()
    {
        m_stopTaskExecutor.store(true);
        m_taskExecutorCondition.notify_all();
        for (auto& t : m_threads)
            if (t.joinable()) t.join();
        m_threads.clear();
    }

    void task::waitForAll()
    {
        std::unique_lock<std::mutex> lock(m_pendingTasksMutex);
        m_pendingTasksCondition.wait(lock, [this] { 
            return m_pendingTasks.load() == 0 && m_tasks.empty(); 
        });
    }

    int task::getTotalPendingTaskCount() const
    {
        return m_pendingTasks.load();
    }

    int task::getQueuedTaskCount() const
    {
        std::lock_guard<std::mutex> lock(m_taskExecutorMutex);
        return m_tasks.size();
    }

    bool task::isRunning() const
    {
        return !m_stopTaskExecutor.load();
    }

    void task::taskExecutor()
    {
        while (true) 
        {
            std::function<void()> task = getTask();
            if (!task)
                return;
            task();
        }
    }

    void task::addTask(std::function<void()> task)
    {
        
        std::lock_guard<std::mutex> lock(m_taskExecutorMutex);
        if (m_stopTaskExecutor.load())
            throw std::runtime_error("MTSLib: addTask called after executor stop");
        m_tasks.push(std::move(task));
    }

    std::function<void()> task::getTask()
    {
        std::function<void()> task;
            
        std::unique_lock<std::mutex> lock(m_taskExecutorMutex);
        m_taskExecutorCondition.wait(lock, [this] { 
            return !m_tasks.empty() || m_stopTaskExecutor.load(); 
        });
        if (m_stopTaskExecutor.load())
            return nullptr;

        task = std::move(m_tasks.front());
        m_tasks.pop();
        return task;
    }
}
