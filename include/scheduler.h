#pragma once

#include "ImtsResource.h"
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <chrono>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace mtsLib
{
    struct schedulerTask
    {
        std::function<void()> taskFunction;
        std::chrono::steady_clock::time_point executeAt;
    };

    struct schedulerTaskComparator
    {
        bool operator()(const schedulerTask& a, const schedulerTask& b) const
        {
            return a.executeAt > b.executeAt; // Earlier executeAt has higher priority
        }
    };

    /**
     * @brief Multithreaded task scheduler with fixed worker threads.
     *
     * The scheduler runs tasks on a fixed number of worker threads inherited
     * from ImtsResource. Tasks are scheduled for execution and workers consume
     * them until the scheduler is stopped.
     */
    class scheduler : public ImtsResource
    {

    public:
        /**
        * @brief Construct a scheduler using the default thread count.
        */
        scheduler();

        /**
        * @brief Construct a scheduler with an explicit worker count.
        * @param threadNumber Number of worker threads to create on execute().
        */
        scheduler(int threadNumber);

        /**
        * @brief Stop all workers and release scheduler resources.
        */
        ~scheduler() override;

        /**
        * @brief Start worker threads.
        * @throws std::runtime_error If called while scheduler is already running.
        */
        void start() override;

        /**
        * @brief Signal workers to stop and join all threads.
        *
        * After this call, workers exit and queued tasks are no longer consumed.
        */
        void stop() override;

        /**
        * @brief Schedule a task for execution and get a future for its result.
        * @tparam F Callable type.
        * @tparam Args Argument types.
        * @param f Callable to execute.
        * @param args Arguments to invoke callable with.
        * @return Future for the result of the scheduled task.
        * @throws std::runtime_error If scheduling occurs after stop().
        */
        template <typename F, typename... Args>
            requires std::invocable<F, Args...>
        auto schedule(F&& f, Args&&... args) ->
            std::future<std::invoke_result_t<F, Args...>>
        {
            return enqueueScheduledTask(std::chrono::steady_clock::duration::zero(),
                                        std::forward<F>(f),
                                        std::forward<Args>(args)...);
        }

        /**
        * @brief Schedule a task for execution after a delay and get a future for its result.
        * @tparam F Callable type.
        * @tparam Args Argument types.
        * @tparam Rep Duration representation type.
        * @tparam Period Duration period type.
        * @param f Callable to execute.
        * @param args Arguments to invoke callable with.
        * @param delay Time to wait before executing the task.
        * @return Future for the result of the scheduled task.
        * @throws std::runtime_error If scheduling occurs after stop().
        */
        template <typename F, typename... Args, typename Rep, typename Period>
            requires std::invocable<F, Args...>
        auto schedule(F&& f, Args&&... args, std::chrono::duration<Rep, Period> delay) ->
            std::future<std::invoke_result_t<F, Args...>>
        {
            return enqueueScheduledTask(std::chrono::duration_cast<std::chrono::steady_clock::duration>(delay),
                                        std::forward<F>(f),
                                        std::forward<Args>(args)...);
        }

    private:
        /**
        * @brief Internal method to enqueue a task with a specified delay.
        * @tparam F Callable type.
        * @tparam Args Argument types.
        * @param delay Time to wait before executing the task.
        * @param f Callable to execute.
        * @param args Arguments to invoke callable with.
        * @return Future for the result of the scheduled task.
        * @throws std::runtime_error If scheduling occurs after stop().
        */
        template <typename F, typename... Args>
        auto enqueueScheduledTask(std::chrono::steady_clock::duration delay, F&& f, Args&&... args) ->
            std::future<std::invoke_result_t<F, Args...>>
        {
            using ReturnType = std::invoke_result_t<F, Args...>;

            if (delay < std::chrono::steady_clock::duration::zero())
            {
                delay = std::chrono::steady_clock::duration::zero();
            }

            auto packaged = std::make_shared<std::packaged_task<ReturnType()>>(
                [fn = std::forward<F>(f),
                 params = std::make_tuple(std::forward<Args>(args)...)]() mutable -> ReturnType
                {
                    return std::apply(std::move(fn), std::move(params));
                }
            );

            std::future<ReturnType> result = packaged->get_future();

            schedulerTask task;
            task.taskFunction = [packaged, this]() {
                (*packaged)();
                m_pendingTasks.fetch_sub(1);
            };
            task.executeAt = std::chrono::steady_clock::now() + delay;

            m_pendingTasks.fetch_add(1);
            addTask(std::move(task));
            m_taskExecutorCondition.notify_one();
            return result;
        }

        /**
        * @brief Main loop for executing scheduled tasks.
        */
        void taskExecutor();

        /**
        * @brief Add a task to the scheduler's queue.
        * @param task Task to add.
        */
        void addTask(schedulerTask&& task);

        /**
        * @brief Retrieve the next task to execute.
        * @return Task function to execute.
        */
        std::function<void()> getTask();

        /** @brief Flag to stop the task executor. */
        std::atomic<bool> m_stopTaskExecutor{true};
        /** @brief Number of pending tasks. */
        std::atomic<int> m_pendingTasks{0};
        /** @brief Priority queue of scheduled tasks. */
        std::priority_queue<schedulerTask, std::vector<schedulerTask>, schedulerTaskComparator> m_taskQueue;
        /** @brief Mutex for protecting access to the task queue. */
        std::mutex m_taskQueueMutex;
        /** @brief Condition variable for task executor threads. */
        std::condition_variable m_taskExecutorCondition;

    };

}