#include "solar_net/net/event_loop_thread_pool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <set>
#include <thread>

namespace solar_net {

TEST(EventLoopThreadPoolTest, StartReturnsAllLoops) {
    EventLoopThreadPool pool(3, "io");
    pool.Start();

    ASSERT_EQ(pool.ThreadCount(), 3u);

    for (size_t i = 0; i < pool.ThreadCount(); ++i) {
        EventLoop* loop = pool.GetLoop(i);
        ASSERT_NE(loop, nullptr);
    }

    pool.Stop();
}

TEST(EventLoopThreadPoolTest, GetNextLoopRoundRobin) {
    EventLoopThreadPool pool(3, "io");
    pool.Start();

    EventLoop* loop0 = pool.GetNextLoop();
    EventLoop* loop1 = pool.GetNextLoop();
    EventLoop* loop2 = pool.GetNextLoop();
    EventLoop* loop3 = pool.GetNextLoop();

    ASSERT_NE(loop0, nullptr);
    ASSERT_NE(loop1, nullptr);
    ASSERT_NE(loop2, nullptr);
    ASSERT_NE(loop3, nullptr);

    EXPECT_NE(loop0, loop1);
    EXPECT_NE(loop1, loop2);
    EXPECT_NE(loop2, loop0);
    EXPECT_EQ(loop0, loop3);

    pool.Stop();
}

TEST(EventLoopThreadPoolTest, GetLoopByIndex) {
    EventLoopThreadPool pool(2, "io");
    pool.Start();

    EventLoop* loop0 = pool.GetLoop(0);
    EventLoop* loop1 = pool.GetLoop(1);

    ASSERT_NE(loop0, nullptr);
    ASSERT_NE(loop1, nullptr);
    EXPECT_NE(loop0, loop1);
    EXPECT_EQ(pool.GetLoop(99), nullptr);

    pool.Stop();
}

TEST(EventLoopThreadPoolTest, IsRunningState) {
    EventLoopThreadPool pool(2, "io");
    EXPECT_FALSE(pool.IsRunning());

    pool.Start();
    EXPECT_TRUE(pool.IsRunning());

    pool.Stop();
    EXPECT_FALSE(pool.IsRunning());
}

TEST(EventLoopThreadPoolTest, ZeroThreadCountTreatedAsOne) {
    EventLoopThreadPool pool(0, "io");
    pool.Start();

    EXPECT_EQ(pool.ThreadCount(), 1u);
    EXPECT_NE(pool.GetNextLoop(), nullptr);

    pool.Stop();
}

TEST(EventLoopThreadPoolTest, MultipleStartNoEffect) {
    EventLoopThreadPool pool(2, "io");
    pool.Start();

    EventLoop* loop0 = pool.GetLoop(0);
    EventLoop* loop1 = pool.GetLoop(1);

    pool.Start(); // should be a no-op

    EXPECT_EQ(pool.GetLoop(0), loop0);
    EXPECT_EQ(pool.GetLoop(1), loop1);
    EXPECT_EQ(pool.ThreadCount(), 2u);

    pool.Stop();
}

TEST(EventLoopThreadPoolTest, StopIsIdempotent) {
    EventLoopThreadPool pool(2, "io");
    pool.Start();
    pool.Stop();
    pool.Stop(); // should not crash or throw

    EXPECT_FALSE(pool.IsRunning());
    EXPECT_EQ(pool.GetNextLoop(), nullptr);
}

TEST(EventLoopThreadPoolTest, GetNextLoopBeforeStartReturnsNull) {
    EventLoopThreadPool pool(2, "io");
    EXPECT_EQ(pool.GetNextLoop(), nullptr);
}

TEST(EventLoopThreadPoolTest, GetNextLoopAfterStopReturnsNull) {
    EventLoopThreadPool pool(2, "io");
    pool.Start();
    pool.Stop();
    EXPECT_EQ(pool.GetNextLoop(), nullptr);
}

TEST(EventLoopThreadPoolTest, InitCallbackCalledForEachThread) {
    std::atomic<int> counter{0};
    EventLoopThreadPool pool(3, "io", [&counter](EventLoop*) {
        counter.fetch_add(1, std::memory_order_relaxed);
    });

    pool.Start();

    EXPECT_EQ(counter.load(std::memory_order_relaxed), 3);

    pool.Stop();
}

TEST(EventLoopThreadPoolTest, ConcurrentGetNextLoop) {
    EventLoopThreadPool pool(4, "io");
    pool.Start();

    std::atomic<int> counter{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < 100; ++j) {
                EventLoop* loop = pool.GetNextLoop();
                if (loop != nullptr) {
                    counter.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(counter.load(std::memory_order_relaxed), 800);

    pool.Stop();
}

} // namespace solar_net
