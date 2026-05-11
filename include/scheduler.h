#pragma once

#include "ImtsResource.h"

namespace mtsLib
{
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

    private:
        std::atomic<bool> m_stopTaskExecutor{false};
    };
}