#include "solar_net/net/event_loop_thread.h"
#include "solar_net/net/event_loop.h"

#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <thread>

int main() {
    std::cout << "Starting EventLoopThread example...\n";

    solar_net::EventLoopThread loop_thread("example");
    solar_net::EventLoop* loop = loop_thread.Start();
    if (loop == nullptr) {
        std::cerr << "Failed to start EventLoopThread\n";
        return 1;
    }

    std::atomic<int> counter{0};

    loop->RunInLoop([&] {
        loop->RunEvery(std::chrono::milliseconds(500), [&] {
            std::cout << std::format("tick {} from {}\n",
                                     counter.fetch_add(1, std::memory_order_relaxed),
                                     loop_thread.GetName());
        });
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "Stopping EventLoopThread...\n";
    loop_thread.Stop();

    std::cout << std::format("final counter = {}\n", counter.load(std::memory_order_relaxed));
    return 0;
}
