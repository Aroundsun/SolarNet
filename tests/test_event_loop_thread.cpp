#include "solar_net/net/event_loop_thread.h"

#include "solar_net/net/event_loop.h"
#include "solar_net/base/thread.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace solar_net {

TEST(EventLoopThreadTest, StartReturnsLoop) {
    EventLoopThread loop_thread("test");
    EventLoop* loop = loop_thread.Start();

    ASSERT_NE(loop, nullptr);
    EXPECT_EQ(loop, loop_thread.GetLoop());

    loop_thread.Stop();
    EXPECT_EQ(loop_thread.GetLoop(), nullptr);
}

TEST(EventLoopThreadTest, InitCallbackRunsInLoopThread) {
    EventLoop* captured_loop = nullptr;
    EventLoopThread loop_thread("test", [&captured_loop](EventLoop* loop) {
        captured_loop = loop;
        EXPECT_TRUE(loop->IsInLoopThread());
    });

    loop_thread.Start();

    EXPECT_EQ(loop_thread.GetLoop(), captured_loop);

    loop_thread.Stop();
}

TEST(EventLoopThreadTest, ThreadNameIsSet) {
    std::string thread_name;
    EventLoopThread loop_thread("named_thread", [&thread_name](EventLoop*) {
        thread_name = Thread::GetCurrentThreadName();
    });

    loop_thread.Start();

    EXPECT_EQ(thread_name, "named_thread");

    loop_thread.Stop();
}

TEST(EventLoopThreadTest, RunInLoopFromOtherThread) {
    EventLoopThread loop_thread("test");
    EventLoop* loop = loop_thread.Start();
    ASSERT_NE(loop, nullptr);

    std::atomic<int> value{0};
    std::thread other([&] {
        loop->RunInLoop([&] { value.fetch_add(1, std::memory_order_relaxed); });
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    other.join();

    loop_thread.Stop();

    EXPECT_EQ(value.load(std::memory_order_relaxed), 1);
}

TEST(EventLoopThreadTest, MultipleStartReturnsSameLoop) {
    EventLoopThread loop_thread("test");

    EventLoop* loop1 = loop_thread.Start();
    EventLoop* loop2 = loop_thread.Start();

    EXPECT_EQ(loop1, loop2);

    loop_thread.Stop();
}

TEST(EventLoopThreadTest, StopIsIdempotent) {
    EventLoopThread loop_thread("test");
    loop_thread.Start();

    loop_thread.Stop();
    loop_thread.Stop();

    EXPECT_EQ(loop_thread.GetLoop(), nullptr);
}

TEST(EventLoopThreadTest, LoopCanQuitItself) {
    EventLoopThread loop_thread("test");
    EventLoop* loop = loop_thread.Start();
    ASSERT_NE(loop, nullptr);

    std::atomic<bool> fired{false};
    loop->RunInLoop([&] {
        loop->RunAfter(std::chrono::milliseconds(10), [&] {
            fired.store(true, std::memory_order_relaxed);
            loop->Quit();
        });
    });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (loop_thread.GetLoop() != nullptr && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    loop_thread.Stop();

    EXPECT_TRUE(fired.load(std::memory_order_relaxed));
    EXPECT_EQ(loop_thread.GetLoop(), nullptr);
}

TEST(EventLoopThreadTest, DestructorStopsRunningThread) {
    std::atomic<bool> task_executed{false};
    {
        EventLoopThread loop_thread("test");
        EventLoop* loop = loop_thread.Start();
        ASSERT_NE(loop, nullptr);

        loop->RunInLoop([&] { task_executed.store(true, std::memory_order_relaxed); });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(task_executed.load(std::memory_order_relaxed));
}

} // namespace solar_net
