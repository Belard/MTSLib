#pragma once

#include "ImtsResource.h"
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
#include <tuple>
#include <barrier>

namespace mtsLib
{
    /**
     * @brief Multithreaded task executor with future-based task submission.
     *
     * The executor runs tasks on a fixed number of worker threads inherited
     * from ImtsResource. Tasks are submitted with add() and observed through
     * futures.
     */
    class task : public ImtsResource
    {
        public:
            /**
             * @brief Construct a task executor using the default thread count.
             */
            task();

            /**
             * @brief Construct a task executor with an explicit worker count.
             * @param threadNumber Number of worker threads to create on execute().
             */
            task(int threadNumber);

            /**
             * @brief Stop all workers and release executor resources.
             */
            ~task() override;

            /**
             * @brief Start worker threads.
             * @throws std::runtime_error If called while executor is already running.
             */
            void start() override;

            /**
            * @brief Syncronize all tasks to start at the same time. (If there more tasks than threads, they will run in batches)
            * @param batch If true, tasks will run in batches of thread count. If false, the tasks will run as much as the threads allow at the same time and close.
            * @throws std::runtime_error If called while executor is already running.
            */
            void synchronizedStart(bool batch = false);

            /**
             * @brief Signal workers to stop and join all threads.
             *
             * After this call, workers exit and queued tasks are no longer consumed.
             */
            void stop() override;

            /**
             * @brief Block until no pending and no queued tasks remain.
             */
            void waitForAll();

            /**
             * @brief Get the current number of pending tasks.
             * @return Pending task count.
             */
            int  getTotalPendingTaskCount() const;

            /**
             * @brief Get the current number of queued tasks.
             * @return Number of tasks currently in the internal queue.
             */
            int  getQueuedTaskCount() const;   

            /**
             * @brief Check whether the executor is in running state.
             * @return true when stop flag is not set, false otherwise.
             */
            bool isRunning() const;

            /**
             * @brief Submit a callable and return a future for its result.
             * @tparam F Callable type.
             * @tparam Args Callable argument types.
             * @param f Callable object.
             * @param args Arguments forwarded to the callable.
             * @return Future associated with callable execution result.
             * @throws std::runtime_error If submission occurs after stop().
             */
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

            /**
             * @brief Submit multiple zero-argument callables.
             * @tparam Fs Callable types.
             * @param fs Callables to submit.
             * @return Tuple of futures in the same order as provided callables.
             */
            template <typename... Fs>
            auto addAll(Fs&&... fs) -> std::tuple<std::future<std::invoke_result_t<Fs>>...>
            {
                return std::make_tuple(add(std::forward<Fs>(fs))...);
            }

        private:
            /** @brief FIFO queue that stores submitted tasks. */
            std::queue<std::function<void()>> m_tasks;

            /** @brief Protects access to m_tasks. */
            mutable std::mutex m_taskExecutorMutex;

            /** @brief Wakes worker threads when tasks are available or stop is requested. */
            std::condition_variable m_taskExecutorCondition;

            /** @brief Synchronizes waitForAll() waiting logic. */
            std::mutex m_pendingTasksMutex;

            /** @brief Notifies waiters when pending task state changes. */
            std::condition_variable m_pendingTasksCondition;

            /** @brief Stop flag used by workers and running-state checks. */
            std::atomic<bool> m_stopTaskExecutor{ true };

            /** @brief Count of tasks pending completion. */
            std::atomic<int> m_pendingTasks{ 0 };

            /** @brief Worker thread loop that fetches and executes tasks. */
            void taskExecutor();

            /** @brief Worker thread loop that fetches and executes tasks in synchronized mode.
             * @param batch If true, tasks will run in batches of thread count. If false, the tasks will run as much as the threads allow at the same time and close.
             * @param taskBarrier Barrier used to synchronize task execution.
             */
            void synchronizedTaskExecutor(bool batch, std::shared_ptr<std::barrier<>> taskBarrier);

            /**
             * @brief Push a task into the queue.
             * @param task Task to queue.
             * @throws std::runtime_error If called after stop().
             */
            void addTask(std::function<void()> task);

            /**
             * @brief Wait for and retrieve next queued task.
             * @return Next task, or nullptr-like callable when stop is requested.
             */
            std::function<void()> getTask();
    };
}