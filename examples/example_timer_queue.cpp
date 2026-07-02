#include "solar_net/net/event_loop.h"

#include <atomic>
#include <chrono>
#include <format>
#include <iostream>

int main() {
    solar_net::EventLoop loop;

    std::atomic<int> counter{0};

    loop.RunAfter(std::chrono::milliseconds(100), [&] {
        std::cout << std::format("one-shot timer fired, counter={}\n", counter.load());
    });

    const auto repeating = loop.RunEvery(std::chrono::milliseconds(50), [&] {
        counter.fetch_add(1, std::memory_order_relaxed);
    });

    loop.RunAfter(std::chrono::milliseconds(300), [&] {
        loop.Cancel(repeating);
        std::cout << std::format("stopping, counter={}\n", counter.load());
        loop.Quit();
    });

    loop.Loop();
    return 0;
}
