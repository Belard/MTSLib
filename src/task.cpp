#include "task.h"

namespace mtsLib
{
    task::task() : ImtsResource() {}
    task::task(int threadNumber) : ImtsResource(threadNumber) {}

    task::~task()
    {
        m_stopTaskExecutor.store(true);
        m_taskExecutorCondition.notify_all();
        for (auto& t : m_threads)
            if (t.joinable()) t.join();
    }

    void task::add(std::function<void()> task)
    {
        addTask(std::move(task));
    }

    void task::add(std::queue<std::function<void()>> tasks)
    {
        while (!tasks.empty())
        {
            addTask(std::move(tasks.front()));
            tasks.pop();
        }
    }

    void task::execute()
    {
        for (auto i = 0; i < m_threadNumber; i++)
        {
            m_threads.emplace_back(&task::taskExecutor, this);
        }
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
        {
            std::unique_lock<std::mutex> lock(m_taskExecutorMutex);
            m_tasks.push(std::move(task));
        }
        m_taskExecutorCondition.notify_one();
    }

    std::function<void()> task::getTask()
    {
        std::function<void()> task;
            
        std::unique_lock<std::mutex> lock(m_taskExecutorMutex);
        m_taskExecutorCondition.wait(lock, [this] { 
            return !m_tasks.empty() || m_stopTaskExecutor.load(); 
        });
        if (m_stopTaskExecutor.load() && m_tasks.empty())
            return nullptr;

        task = std::move(m_tasks.front());
        m_tasks.pop();
        return task;
    }
}
