#include "solar_net/base/thread.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace solar_net;

TEST(ThreadTest, RunsCallback) {
    std::atomic<int> value{0};
    Thread thread([&value] { value.store(42, std::memory_order_relaxed); });
    thread.Start();
    thread.Join();

    EXPECT_EQ(value.load(std::memory_order_relaxed), 42);
}

TEST(ThreadTest, IsStartedAfterStart) {
    Thread thread([] {});
    EXPECT_FALSE(thread.IsStarted());
    thread.Start();
    EXPECT_TRUE(thread.IsStarted());
    thread.Join();
}

TEST(ThreadTest, GetIdReturnsValidId) {
    Thread thread([] {});
    thread.Start();
    const std::thread::id id = thread.GetId();
    EXPECT_NE(id, std::thread::id{});
    thread.Join();
}

TEST(ThreadTest, DestructorAutoJoins) {
    std::atomic<bool> finished{false};
    {
        Thread thread([&finished] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            finished.store(true, std::memory_order_relaxed);
        });
        thread.Start();
        // No explicit Join; destructor should join.
    }
    EXPECT_TRUE(finished.load(std::memory_order_relaxed));
}

TEST(ThreadTest, NameIsAccessible) {
    Thread thread([] {}, "test-thread");
    EXPECT_EQ(thread.GetName(), "test-thread");
}

TEST(ThreadTest, CurrentThreadNameRoundTrip) {
    Thread::SetCurrentThreadName("main-test");
    // pthread_getname_np may truncate to 15 bytes.
    const std::string name = Thread::GetCurrentThreadName();
    EXPECT_FALSE(name.empty());
    EXPECT_EQ(name.substr(0, std::min(name.size(), size_t{9})), "main-test");
}
