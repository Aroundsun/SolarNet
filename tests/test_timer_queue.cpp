#include "solar_net/net/event_loop.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>

namespace solar_net {

TEST(TimerQueueTest, RunAfterFiresOnce) {
    EventLoop loop;

    std::atomic<bool> fired{false};
    loop.RunAfter(std::chrono::milliseconds(10), [&] { fired.store(true, std::memory_order_relaxed); });
    loop.RunAfter(std::chrono::milliseconds(50), [&] { loop.Quit(); });

    loop.Loop();

    EXPECT_TRUE(fired.load(std::memory_order_relaxed));
}

TEST(TimerQueueTest, RunEveryFiresMultipleTimes) {
    EventLoop loop;

    std::atomic<int> counter{0};
    const auto id = loop.RunEvery(std::chrono::milliseconds(10), [&] {
        counter.fetch_add(1, std::memory_order_relaxed);
    });

    loop.RunAfter(std::chrono::milliseconds(55), [&] {
        loop.Cancel(id);
        loop.Quit();
    });

    loop.Loop();

    EXPECT_GE(counter.load(std::memory_order_relaxed), 3);
}

TEST(TimerQueueTest, CancelPreventsFiring) {
    EventLoop loop;

    std::atomic<bool> fired{false};
    const auto id = loop.RunAfter(std::chrono::milliseconds(100), [&] {
        fired.store(true, std::memory_order_relaxed);
    });

    loop.RunAfter(std::chrono::milliseconds(10), [&] {
        EXPECT_TRUE(loop.Cancel(id));
    });
    loop.RunAfter(std::chrono::milliseconds(50), [&] { loop.Quit(); });

    loop.Loop();

    EXPECT_FALSE(fired.load(std::memory_order_relaxed));
}

TEST(TimerQueueTest, CancelInvalidReturnsFalse) {
    EventLoop loop;

    EXPECT_FALSE(loop.Cancel(9999));
    loop.RunAfter(std::chrono::milliseconds(10), [&] { loop.Quit(); });
    loop.Loop();
}

} // namespace solar_net
