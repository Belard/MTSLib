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

    }

    void scheduler::stop()
    {
        m_stopTaskExecutor.store(true);
        for (auto& t : m_threads)
            if (t.joinable()) t.join();
        m_threads.clear();
    }
}