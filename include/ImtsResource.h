#pragma once

#include <vector>
#include <thread>


class ImtsResource
{
public:
    ImtsResource() : m_threadNumber(std::thread::hardware_concurrency()) {}
    ImtsResource(int threadNumber) : m_threadNumber(threadNumber) {};
    virtual ~ImtsResource() = default;
    virtual void execute() = 0;
    virtual void stop() = 0;

protected:
    int                         m_threadNumber;
    std::vector<std::thread>    m_threads;
};