#include <iostream>
#include <atomic>
#include <chrono>
#include <queue>
#include <thread>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>

#include "ImtsResource.h"
#include "task.h"

int main()
{
    // --- Test 1: Single task add ---
    {
        std::cout << "[Test 1] Single task execution\n";

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

    // --- Test 2: Batch task add via queue ---
    {
        std::cout << "[Test 2] Batch task execution via queue\n";

        std::atomic<int> counter{ 0 };
        mtsLib::task t(4);
        t.execute();

        std::queue<std::function<void()>> batch;
        for (int i = 0; i < 8; i++)
        {
            batch.push([&counter, i]() {
                std::cout << "  Batch task " << i << " running\n";
                counter.fetch_add(1);
            });
        }

        t.addAll(std::move(batch));

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "  Batch tasks completed: " << counter.load() << "/8\n\n";
    }

    // --- Test 3: Default thread count (hardware_concurrency) ---
    {
        std::cout << "[Test 3] Default thread count ("
                  << std::thread::hardware_concurrency() << " threads)\n";

        std::atomic<int> counter{ 0 };
        mtsLib::task t;
        t.execute();

        for (int i = 0; i < 10; i++)
        {
            t.add([&counter]() {
                counter.fetch_add(1);
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "  Tasks completed: " << counter.load() << "/10\n\n";
    }

    std::cout << "All tests done.\n";
    return 0;
}
