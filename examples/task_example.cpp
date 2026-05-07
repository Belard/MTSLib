#include <iostream>
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
        t.execute();

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
        t.execute();

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
        t.execute();

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
        t.execute();

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

    // --- Test 5: stop() function demonstration (execute -> stop -> execute) ---
    {
        std::cout << "[Test 5] stop() and re-execute demonstration\n";

        std::atomic<int> completed{ 0 };
        mtsLib::task t(2);

        // Phase 1: First execute() and add tasks
        std::cout << "  Phase 1: First execute()\n";
        t.execute();

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

        // Phase 3: Execute again and add more tasks
        std::cout << "  Phase 3: Second execute() after stop()\n";
        completed.store(0);
        t.execute();

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

        std::cout << "  Before execute(): isRunning() = " << std::boolalpha << t.isRunning() << "\n";
        t.execute();
        std::cout << "  After  execute(): isRunning() = " << t.isRunning() << "\n";
        t.stop();
        std::cout << "  After  stop()   : isRunning() = " << t.isRunning() << "\n\n";
    }

    // --- Test 7: getQueuedTaskCount() and getTotalPendingTaskCount() (simple) ---
    {
        std::cout << "[Test 7] getQueuedTaskCount() and getTotalPendingTaskCount() - simple\n";

        mtsLib::task t(1); // single thread so we can reason about queue depth
        t.execute();

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
        t.execute();

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
        t.execute();

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

    std::cout << "All tests done.\n";
    return 0;
}
