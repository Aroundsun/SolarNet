#include "solar_net/base/thread_pool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace solar_net;

TEST(ThreadPoolTest, RunsTasks) {
    ThreadPool pool(2);
    pool.Start();

    std::atomic<int> counter{0};
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(pool.Submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
    }

    pool.Stop();
    pool.Wait();
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 10);
}

TEST(ThreadPoolTest, SubmitFailsAfterStop) {
    ThreadPool pool(1);
    pool.Start();
    pool.Stop();
    pool.Wait();

    EXPECT_FALSE(pool.Submit([] {}));
}

TEST(ThreadPoolTest, PendingTaskCountTracksTasks) {
    ThreadPool pool(1);
    pool.Start();

    std::atomic<bool> blocker{true};
    std::atomic<bool> started{false};
    pool.Submit([&] {
        started.store(true, std::memory_order_relaxed);
        while (blocker.load(std::memory_order_relaxed)) {
            std::this_thread::yield();
        }
    });

    // Wait for the worker to pick up the first task.
    while (!started.load(std::memory_order_relaxed)) {
        std::this_thread::yield();
    }

    // One task is running; submit another and verify it is pending.
    EXPECT_TRUE(pool.Submit([] {}));
    // Give a short moment for the task to be enqueued.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_GE(pool.PendingTaskCount(), 1);

    blocker.store(false, std::memory_order_relaxed);
    pool.Stop();
    pool.Wait();
}

TEST(ThreadPoolTest, DestructorAutoStopsAndWaits) {
    std::atomic<int> counter{0};
    {
        ThreadPool pool(2);
        pool.Start();
        for (int i = 0; i < 5; ++i) {
            pool.Submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
        }
    }
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 5);
}

TEST(ThreadPoolTest, TaskExceptionDoesNotCrashPool) {
    ThreadPool pool(2);
    pool.Start();

    std::atomic<int> counter{0};
    pool.Submit([] { throw std::runtime_error("expected"); });
    pool.Submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });

    pool.Stop();
    pool.Wait();
    EXPECT_EQ(counter.load(std::memory_order_relaxed), 1);
}

TEST(ThreadPoolTest, ReportsCorrectThreadCount) {
    ThreadPool pool(4);
    EXPECT_EQ(pool.ThreadCount(), 4);
}
