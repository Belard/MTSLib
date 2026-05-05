#pragma once

#include "ImtsResource.h"
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace mtsLib
{
    class task : public ImtsResource
    {
        public:
            task();
            task(int threadNumber);
            ~task();

            void add(std::function<void()> task);
            void add(std::queue<std::function<void()>> tasks);
            void execute();
        private:
            std::queue<std::function<void()>> m_tasks;

            std::mutex m_taskExecutorMutex;
            std::condition_variable m_taskExecutorCondition;
            std::atomic<bool> m_stopTaskExecutor{ false };

            void taskExecutor();
            void addTask(std::function<void()> task);
            std::function<void()> getTask();
    };
}