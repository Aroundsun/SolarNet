#include "solar_net/net/event_loop_thread_pool.h"
#include "solar_net/net/event_loop.h"

#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <thread>

int main() {
    std::cout << "Starting EventLoopThreadPool example with 3 threads...\n";

    solar_net::EventLoopThreadPool pool(3, "worker");
    pool.Start();

    std::cout << std::format("pool has {} threads\n", pool.ThreadCount());

    std::atomic<int> counter{0};

    for (size_t i = 0; i < pool.ThreadCount(); ++i) {
        solar_net::EventLoop* loop = pool.GetLoop(i);
        if (loop == nullptr) {
            continue;
        }

        loop->RunInLoop([loop, i, &counter] {
            loop->RunAfter(std::chrono::milliseconds(static_cast<int>(i) * 100), [loop, &counter] {
                loop->RunEvery(std::chrono::milliseconds(500), [&] {
                    std::cout << std::format("tick {}\n",
                                             counter.fetch_add(1, std::memory_order_relaxed));
                });
            });
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "Stopping EventLoopThreadPool...\n";
    pool.Stop();

    std::cout << std::format("final counter = {}\n", counter.load(std::memory_order_relaxed));
    return 0;
}
