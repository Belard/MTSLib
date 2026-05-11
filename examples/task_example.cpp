#include <iostream>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>
#include <vector>

#include "ImtsResource.h"
#include "task.h"

// Helper functions for demonstrating add() with arguments
int add_numbers(int a, int b)
{
    return a + b;
}

std::string concat_strings(const std::string& a, const std::string& b)
{
    return a + b;
}

int main()
{
    // --- Test 1: Single task with add() ---
    {
        std::cout << "[Test 1] Single task execution with add()\n";

        std::atomic<int> counter{ 0 };
        mtsLib::task t(2);
        t.start();

        for (int i = 0; i < 5; i++)
        {
            t.add([&counter, i]() {
                std::cout << "  Task " << i << " running on thread "
                          << std::this_thread::get_id() << "\n";
                counter.fetch_add(1);
            });
        }

        // Give workers time to finish
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "  Tasks completed: " << counter.load() << "/5\n\n";
    }

    // --- Test 2: addAll() with multiple variadic functions ---
    {
        std::cout << "[Test 2] Multiple tasks with addAll()\n";

        mtsLib::task t(4);
        t.start();

        // Create multiple lambda functions and pass to addAll()
        auto futures = t.addAll(
            []() {
                std::cout << "  Task A running\n";
                return 1;
            },
            []() {
                std::cout << "  Task B running\n";
                return 2;
            },
            []() {
                std::cout << "  Task C running\n";
                return 3;
            }
        );

        auto [a, b, c] = std::move(futures);
        std::cout << "  Results: " << a.get() << ", " << b.get() << ", " << c.get() << "\n\n";
    }

    // --- Test 3: add() with function and arguments ---
    {
        std::cout << "[Test 3] Task with function and arguments\n";

        mtsLib::task t(2);
        t.start();

        // Pass function pointer with arguments
        auto future_sum = t.add(add_numbers, 10, 20);
        auto future_concat = t.add(concat_strings, std::string("Hello "), std::string("World"));

        std::cout << "  Sum result: " << future_sum.get() << "\n";
        std::cout << "  Concat result: " << future_concat.get() << "\n\n";
    }

    // --- Test 4: Default thread count (hardware_concurrency) ---
    {
        std::cout << "[Test 4] Default thread count ("
                  << std::thread::hardware_concurrency() << " threads)\n";

        std::atomic<int> counter{ 0 };
        mtsLib::task t;
        t.start();

        for (int i = 0; i < 9; i++)
        {
            t.add([&counter]() {
                counter.fetch_add(1);
            });
        }

        auto future = t.add([&counter]() {
                counter.fetch_add(1);
            });

        future.get(); // Wait for the last task to complete
        std::cout << "  Tasks completed: " << counter.load() << "/10\n\n";
    }

    // --- Test 5: stop() function demonstration (start -> stop -> start) ---
    {
        std::cout << "[Test 5] stop() and re-start demonstration\n";

        std::atomic<int> completed{ 0 };
        mtsLib::task t(2);

        // Phase 1: First start() and add tasks
        std::cout << "  Phase 1: First start()\n";
        t.start();

        for (int i = 0; i < 3; i++)
        {
            t.add([&completed, i]() {
                std::cout << "    Phase 1 - Task " << i << " completed\n";
                completed.fetch_add(1);
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "  Phase 1 finished: " << completed.load() << "/3 tasks completed\n\n";

        // Phase 2: Stop the executor
        std::cout << "  Phase 2: Calling stop()\n";
        t.stop();
        std::cout << "  Executor stopped. Threads joined.\n\n";

        // Phase 3: Start again and add more tasks
        std::cout << "  Phase 3: Second start() after stop()\n";
        completed.store(0);
        t.start();

        std::cout << "  Adding new tasks after restart...\n";
        for (int i = 0; i < 3; i++)
        {
            t.add([i]() {
                std::cout << "    Phase 3 - Task " << i << " completed\n";
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "  Phase 3 finished: Executor restarted and ran new tasks\n\n";

        // Phase 4: Final stop
        std::cout << "  Phase 4: Final stop()\n";
        t.stop();
        std::cout << "  Executor stopped again.\n\n";
    }

    // --- Test 6: isRunning() state checks ---
    {
        std::cout << "[Test 6] isRunning() state checks\n";

        mtsLib::task t(2);

        std::cout << "  Before start(): isRunning() = " << std::boolalpha << t.isRunning() << "\n";
        t.start();
        std::cout << "  After  start(): isRunning() = " << t.isRunning() << "\n";
        t.stop();
        std::cout << "  After  stop()   : isRunning() = " << t.isRunning() << "\n\n";
    }

    // --- Test 7: getQueuedTaskCount() and getTotalPendingTaskCount() (simple) ---
    {
        std::cout << "[Test 7] getQueuedTaskCount() and getTotalPendingTaskCount() - simple\n";

        mtsLib::task t(1); // single thread so we can reason about queue depth
        t.start();

        // Block the single worker so queued tasks pile up
        std::mutex gate;
        std::unique_lock<std::mutex> hold(gate);

        std::condition_variable cv;
        std::atomic<bool> workerBlocked{ false };

        // First task holds the worker
        t.add([&gate, &cv, &workerBlocked]() {
            workerBlocked.store(true);
            cv.notify_all();
            std::unique_lock<std::mutex> lk(gate); // blocks until hold is released
        });

        // Wait until the worker is inside the blocking task
        {
            std::mutex waitMtx;
            std::unique_lock<std::mutex> wlk(waitMtx);
            cv.wait(wlk, [&workerBlocked] { return workerBlocked.load(); });
        }

        // Now enqueue 3 more tasks while the worker is blocked
        for (int i = 0; i < 3; i++)
            t.add([i]() { /* no-op */ });

        std::cout << "  Queue depth (getQueuedTaskCount):       " << t.getQueuedTaskCount() << " (expected 3)\n";
        std::cout << "  Running tasks (getTotalPendingTaskCount): " << t.getTotalPendingTaskCount() << " (expected 1+)\n";

        // Release the gate so the worker can finish
        hold.unlock();
        t.waitForAll();

        std::cout << "  After waitForAll() - queue depth:    " << t.getQueuedTaskCount() << " (expected 0)\n";
        std::cout << "  After waitForAll() - running tasks:  " << t.getTotalPendingTaskCount() << " (expected 0)\n\n";
    }

    // --- Test 8: waitForAll() - harder concurrent producer case ---
    {
        std::cout << "[Test 8] waitForAll() with concurrent producers\n";

        mtsLib::task t(4);
        t.start();

        constexpr int producerCount = 4;
        constexpr int tasksPerProducer = 25;
        std::atomic<int> completed{ 0 };

        // Multiple producer threads each submit tasks
        std::vector<std::thread> producers;
        producers.reserve(producerCount);
        for (int p = 0; p < producerCount; p++)
        {
            producers.emplace_back([&t, &completed, p]() {
                for (int i = 0; i < tasksPerProducer; i++)
                {
                    t.add([&completed, p, i]() {
                        // Simulate varying work durations
                        std::this_thread::sleep_for(std::chrono::microseconds((p + 1) * 10));
                        completed.fetch_add(1);
                    });
                }
            });
        }
        for (auto& p : producers) p.join();

        std::cout << "  All producers submitted " << producerCount * tasksPerProducer << " tasks. Waiting...\n";
        t.waitForAll();

        const int total = producerCount * tasksPerProducer;
        std::cout << "  Completed: " << completed.load() << "/" << total
                  << (completed.load() == total ? "  [PASS]" : "  [FAIL]") << "\n";
        std::cout << "  Pending queue after waitForAll(): " << t.getQueuedTaskCount() << "\n";
        std::cout << "  Running tasks after waitForAll(): " << t.getTotalPendingTaskCount() << "\n\n";
    }

    // --- Test 9: waitForAll() + add() futures - harder pipeline ---
    {
        std::cout << "[Test 9] waitForAll() with future results pipeline\n";

        mtsLib::task t(4);
        t.start();

        constexpr int n = 10;
        std::vector<std::future<int>> futures;
        futures.reserve(n);

        // Stage 1: compute squares
        for (int i = 1; i <= n; i++)
            futures.push_back(t.add([i]() -> int {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                return i * i;
            }));

        t.waitForAll();
        std::cout << "  Stage 1 (squares): ";
        int sum = 0;
        for (auto& f : futures)
        {
            int v = f.get();
            sum += v;
            std::cout << v << " ";
        }
        std::cout << "\n  Sum of squares 1..10 = " << sum << " (expected 385)\n";

        // Stage 2: submit a single aggregation task and await it
        auto agg = t.add([sum]() -> std::string {
            return sum == 385 ? "Correct!" : "Wrong!";
        });
        std::cout << "  Aggregation result: " << agg.get() << "\n\n";
    }

    // --- Test 10: synchronizedStart() - batch=false (default) ---
    // Each worker picks up exactly one task, all tasks wait for every thread to
    // be ready before any of them starts executing, then threads exit.
    {
        std::cout << "[Test 10] synchronizedStart() - batch=false (one-shot synchronized start)\n";

        constexpr int threadCount = 4;
        mtsLib::task t(threadCount);

        std::vector<std::chrono::steady_clock::time_point> startTimes(threadCount);
        std::atomic<int> index{ 0 };

        // Queue exactly threadCount tasks before calling synchronizedStart so
        // every worker has a task to pick up before the latch fires.
        for (int i = 0; i < threadCount; i++)
        {
            t.add([&startTimes, &index]() {
                int slot = index.fetch_add(1);
                startTimes[slot] = std::chrono::steady_clock::now();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            });
        }

        t.synchronizedStart(false); // batch=false: threads exit after one task

        t.waitForAll();

        // Measure spread between the earliest and latest start time
        auto minT = *std::min_element(startTimes.begin(), startTimes.end());
        auto maxT = *std::max_element(startTimes.begin(), startTimes.end());
        auto spreadUs = std::chrono::duration_cast<std::chrono::microseconds>(maxT - minT).count();
        std::cout << "  All " << threadCount << " tasks ran simultaneously.\n";
        std::cout << "  Start-time spread: " << spreadUs << " us (should be small)\n\n";
    }

    // --- Test 11: synchronizedStart(true) - batch=true ---
    // Workers stay alive and process tasks in synchronized batches of threadCount.
    {
        std::cout << "[Test 11] synchronizedStart(true) - batched synchronized execution\n";

        constexpr int threadCount = 3;
        constexpr int totalTasks  = 9; // 3 full batches
        mtsLib::task t(threadCount);

        std::atomic<int> completed{ 0 };
        std::atomic<int> batchIndex{ 0 };

        // Queue all tasks upfront
        for (int i = 0; i < totalTasks; i++)
        {
            t.add([&completed, &batchIndex, i, threadCount]() {
                int batch = i / threadCount;
                std::cout << "  Batch " << batch << " - task " << i << " running on thread "
                          << std::this_thread::get_id() << "\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                completed.fetch_add(1);
            });
        }

        t.synchronizedStart(true); // batch=true: workers keep running

        std::cout << "  All tasks submitted. Waiting for completion...\n";

        t.waitForAll();
        t.stop();

        std::cout << "  Completed: " << completed.load() << "/" << totalTasks
                  << (completed.load() == totalTasks ? "  [PASS]" : "  [FAIL]") << "\n\n";
    }

    std::cout << "All tests done.\n";
    return 0;
}
