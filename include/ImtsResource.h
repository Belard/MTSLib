#pragma once

#include <vector>
#include <thread>


class ImtsResource
{
public:
    ImtsResource() : m_threadNumber(std::thread::hardware_concurrency()) {}
    ImtsResource(int threadNumber) : m_threadNumber(threadNumber) {};
    virtual ~ImtsResource() = default;


public:
    int                         m_threadNumber;
    std::vector<std::thread>    m_threads;
};