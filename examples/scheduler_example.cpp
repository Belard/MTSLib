#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "scheduler.h"

namespace
{
    using Clock = std::chrono::steady_clock;

    bool reportCheck(bool condition, const std::string& name)
    {
        std::cout << "  [" << (condition ? "PASS" : "FAIL") << "] " << name << "\n";
        return condition;
    }

    template <typename T>
    void printValue(const std::string& label, const T& value)
    {
        std::cout << "    " << label << ": " << value << "\n";
    }
}

int main()
{
    using namespace std::chrono_literals;

    std::cout << "MTSLib scheduler example\n";
    std::cout << "========================\n\n";

    bool allPassed = true;

    mtsLib::scheduler sched(4);

    // Test 1: scheduler lifecycle and immediate tasks.
    std::cout << "[Test 1] Start, schedule immediate work, gather futures\n";
    sched.start();

    auto f1 = sched.schedule([] { return 7; });
    auto f2 = sched.schedule([](int a, int b) { return a + b; }, 10, 32);
    auto f3 = sched.schedule([] {
        std::this_thread::sleep_for(20ms);
        return std::string("done");
    });

    allPassed &= reportCheck(f1.get() == 7, "simple future result");
    allPassed &= reportCheck(f2.get() == 42, "function with arguments");
    allPassed &= reportCheck(f3.get() == "done", "string result after async work");
    std::cout << "\n";

    // Test 2: delayed scheduling and execution order.
    std::cout << "[Test 2] Delayed scheduling and order\n";
    std::mutex orderMutex;
    std::vector<int> order;

    auto delayedStart = Clock::now();
    auto first = sched.schedule([&] {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(1);
        return Clock::now();
    }, 50ms);

    auto second = sched.schedule([&] {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(2);
        return Clock::now();
    }, 120ms);

    auto t1 = first.get();
    auto t2 = second.get();

    const auto firstDelay = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - delayedStart).count();
    const auto secondDelay = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - delayedStart).count();

    printValue("first delay (ms)", firstDelay);
    printValue("second delay (ms)", secondDelay);

    allPassed &= reportCheck(firstDelay >= 40, "first delayed task ran after ~50ms");
    allPassed &= reportCheck(secondDelay >= 100, "second delayed task ran after ~120ms");
    allPassed &= reportCheck(order.size() == 2 && order[0] == 1 && order[1] == 2,
                             "tasks executed in increasing executeAt order");
    std::cout << "\n";

    // Test 3: exception propagation via future.
    std::cout << "[Test 3] Exception propagation\n";
    auto exceptional = sched.schedule([]() -> int {
        throw std::runtime_error("intentional scheduler task error");
    });

    bool exceptionCaught = false;
    try
    {
        (void)exceptional.get();
    }
    catch (const std::runtime_error& ex)
    {
        exceptionCaught = std::string(ex.what()) == "intentional scheduler task error";
        printValue("exception message", ex.what());
    }

    allPassed &= reportCheck(exceptionCaught, "future.get() rethrows task exception");
    std::cout << "\n";

    // Test 4: concurrent execution showcase.
    std::cout << "[Test 4] Concurrent execution across worker threads\n";
    constexpr int taskCount = 8;
    std::mutex threadSetMutex;
    std::set<std::thread::id> workerIds;
    std::vector<std::future<void>> futures;
    futures.reserve(taskCount);

    auto parallelStart = Clock::now();
    for (int i = 0; i < taskCount; ++i)
    {
        futures.push_back(sched.schedule([&] {
            {
                std::lock_guard<std::mutex> lock(threadSetMutex);
                workerIds.insert(std::this_thread::get_id());
            }
            std::this_thread::sleep_for(120ms);
        }));
    }

    for (auto& fut : futures)
    {
        fut.get();
    }

    const auto parallelDuration =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - parallelStart).count();

    printValue("distinct worker threads used", workerIds.size());
    printValue("total duration (ms)", parallelDuration);

    allPassed &= reportCheck(workerIds.size() >= 2, "work distributed across multiple threads");
    allPassed &= reportCheck(parallelDuration < 600,
                             "parallel run faster than sequential baseline (~960ms)");
    std::cout << "\n";

    // Test 5: stop() then start() again.
    std::cout << "[Test 5] Restart behavior\n";
    sched.stop();

    bool threwOnDoubleStart = false;
    sched.start();
    try
    {
        sched.start();
    }
    catch (const std::runtime_error& ex)
    {
        threwOnDoubleStart = true;
        printValue("double start error", ex.what());
    }

    auto afterRestart = sched.schedule([] { return 99; });
    allPassed &= reportCheck(threwOnDoubleStart, "start() rejects already-running state");
    allPassed &= reportCheck(afterRestart.get() == 99, "scheduler runs tasks after restart");

    sched.stop();

    // Test 6: stop() behavior with running tasks.
    std::cout << "[Test 6] stop() behavior with running tasks\n";

    mtsLib::scheduler stopTestScheduler(4);
    stopTestScheduler.start();

    std::atomic<int> completedTasks = 0;
    constexpr int totalTasks = 6;

    // Schedule tasks, some of which will take time to complete.
    for (int i = 0; i < totalTasks; ++i)
    {
        stopTestScheduler.schedule([&, i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * (i + 1)));
            ++completedTasks;
        });
    }

    // Stop the scheduler after a short delay.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stopTestScheduler.stop();

    // Verify that tasks already in progress completed.
    printValue("tasks completed after stop()", completedTasks.load());
    allPassed &= reportCheck(completedTasks.load() >= 2 && completedTasks.load() < totalTasks,
                             "stop() allows in-progress tasks to finish but no new tasks start");

    std::cout << "\nSummary: " << (allPassed ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED") << "\n";
    return allPassed ? 0 : 1;
}
