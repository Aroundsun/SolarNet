#include "solar_net/net/event_loop.h"

#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <thread>

int main() {
    solar_net::EventLoop loop;

    std::atomic<int> counter{0};

    loop.RunInLoop([&] {
        std::cout << std::format("RunInLoop in loop thread: {}\n", loop.IsInLoopThread());
        counter.fetch_add(1, std::memory_order_relaxed);
    });

    std::thread worker([&] {
        loop.QueueInLoop([&] {
            std::cout << std::format("QueueInLoop from other thread: {}\n", loop.IsInLoopThread());
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    });

    std::thread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop.Quit();
    });

    loop.Loop();

    worker.join();
    stopper.join();

    std::cout << std::format("counter = {}\n", counter.load());
    return 0;
}
