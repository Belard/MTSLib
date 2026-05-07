#pragma once

#include "ImtsResource.h"
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <tuple>

namespace mtsLib
{
    class task : public ImtsResource
    {
        public:
            task();
            task(int threadNumber);
            ~task();

            void execute();
            void stop();
            void waitForAll();
            int  getTotalPendingTaskCount() const;
            int  getQueuedTaskCount() const;   
            bool isRunning() const;

            template <typename F, typename... Args>
            auto add(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
            {
                using ReturnType = std::invoke_result_t<F, Args...>;

                auto packaged = std::make_shared<std::packaged_task<ReturnType()>>(
                    [fn = std::forward<F>(f),
                     params = std::make_tuple(std::forward<Args>(args)...)]() mutable -> ReturnType
                    {
                        return std::apply(std::move(fn), std::move(params));
                    }
                );

                std::future<ReturnType> result = packaged->get_future();

                m_pendingTasks.fetch_add(1);
                addTask([packaged, this]() { 
                    (*packaged)(); 
                    m_pendingTasks.fetch_sub(1);
                    m_pendingTasksCondition.notify_all();
                });
                m_taskExecutorCondition.notify_one();
                return result;
            }

            template <typename... Fs>
            auto addAll(Fs&&... fs) -> std::tuple<std::future<std::invoke_result_t<Fs>>...>
            {
                return std::make_tuple(add(std::forward<Fs>(fs))...);
            }

        private:
            std::queue<std::function<void()>> m_tasks;

            mutable std::mutex m_taskExecutorMutex;
            std::condition_variable m_taskExecutorCondition;
            std::mutex m_pendingTasksMutex;
            std::condition_variable m_pendingTasksCondition;
            std::atomic<bool> m_stopTaskExecutor{ false };
            std::atomic<int> m_pendingTasks{ 0 };

            void taskExecutor();
            void addTask(std::function<void()> task);
            std::function<void()> getTask();
    };
}