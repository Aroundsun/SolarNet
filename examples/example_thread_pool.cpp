#include "solar_net/base/thread_pool.h"

#include <atomic>
#include <format>
#include <iostream>

int main() {
    solar_net::ThreadPool pool(4, "demo");
    pool.Start();

    std::atomic<int> counter{0};
    constexpr int kTasks = 20;

    for (int i = 0; i < kTasks; ++i) {
        pool.Submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
    }

    pool.Stop();
    pool.Wait();

    std::cout << std::format("{} tasks finished, counter = {}\n", kTasks, counter.load());
    return 0;
}
