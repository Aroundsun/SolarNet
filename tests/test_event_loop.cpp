#include "solar_net/net/event_loop.h"

#include "solar_net/net/channel.h"
#include "solar_net/base/time.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <unistd.h>

namespace solar_net {

TEST(EventLoopTest, IsInLoopThread) {
    EventLoop loop;
    EXPECT_TRUE(loop.IsInLoopThread());

    std::atomic<bool> in_loop_thread{false};
    std::thread other([&] {
        loop.RunInLoop([&] { in_loop_thread = loop.IsInLoopThread(); });
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    loop.Quit();
    loop.Loop();

    other.join();
    EXPECT_TRUE(in_loop_thread.load());
}

TEST(EventLoopTest, RunInLoopFromSameThread) {
    EventLoop loop;

    std::atomic<int> value{0};
    loop.RunInLoop([&] { value.fetch_add(1, std::memory_order_relaxed); });
    EXPECT_EQ(value.load(std::memory_order_relaxed), 1);
}

TEST(EventLoopTest, QueueInLoopFromOtherThread) {
    EventLoop loop;

    std::atomic<int> value{0};
    std::thread other([&] {
        loop.QueueInLoop([&] { value.fetch_add(1, std::memory_order_relaxed); });
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    loop.Quit();
    loop.Loop();

    other.join();
    EXPECT_EQ(value.load(std::memory_order_relaxed), 1);
}

TEST(EventLoopTest, QuitFromOtherThread) {
    EventLoop loop;

    std::thread other([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        loop.Quit();
    });

    loop.Loop();
    other.join();
}

TEST(EventLoopTest, ChannelIntegration) {
    EventLoop loop;

    int fds[2];
    ASSERT_EQ(pipe(fds), 0);

    Channel channel(&loop, fds[0]);
    std::atomic<int> read_count{0};
    channel.SetReadCallback([&](Time) { read_count.fetch_add(1, std::memory_order_relaxed); });
    channel.EnableReading();

    const char c = 'x';
    ASSERT_EQ(write(fds[1], &c, sizeof(c)), sizeof(c));

    std::thread other([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        loop.Quit();
    });

    loop.Loop();

    other.join();
    channel.DisableAll();
    loop.RemoveChannel(&channel);
    close(fds[0]);
    close(fds[1]);

    EXPECT_GE(read_count.load(std::memory_order_relaxed), 1);
}

TEST(EventLoopTest, NextTimeoutReflectsTimers) {
    EventLoop loop;

    EXPECT_EQ(loop.NextTimeout(), -1);

    loop.RunAfter(std::chrono::milliseconds(200), [] {});
    const int timeout = loop.NextTimeout();
    EXPECT_GE(timeout, 0);
    EXPECT_LE(timeout, 200);

    loop.Cancel(loop.RunAfter(std::chrono::milliseconds(500), [] {}));
    const int after_cancel = loop.NextTimeout();
    EXPECT_GE(after_cancel, 0);
    EXPECT_LE(after_cancel, 200);
}

} // namespace solar_net
