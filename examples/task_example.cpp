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

    std::cout << "All tests done.\n";
    return 0;
}
