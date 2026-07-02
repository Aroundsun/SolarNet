#include "solar_net/base/time.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace solar_net {
namespace {

TEST(TimeTest, NowReturnsCurrentTime) {
    const auto before = std::chrono::system_clock::now();
    const auto now = Time::Now();
    const auto after = std::chrono::system_clock::now();

    EXPECT_GE(now.GetTimePoint(), before);
    EXPECT_LE(now.GetTimePoint(), after);
}

TEST(TimeTest, MillisecondsSinceEpochIsPositive) {
    const auto now = Time::Now();
    EXPECT_GT(now.MillisecondsSinceEpoch().count(), 0);
}

TEST(TimeTest, LogStringContainsDateAndTime) {
    const auto now = Time::Now();
    const auto log_str = now.ToLogString();

    EXPECT_THAT(log_str, testing::MatchesRegex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})"));
}

TEST(TimeTest, FormattedStringUsesFormat) {
    const auto time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto now = Time::Now();
    const auto formatted = now.ToFormattedString("%Y-%m-%d");

    EXPECT_THAT(formatted, testing::MatchesRegex(R"(\d{4}-\d{2}-\d{2})"));
}

TEST(TimeTest, ComparisonWorks) {
    const auto t1 = Time::Now();
    const auto t2 = Time::Now();

    EXPECT_LE(t1, t2);
    EXPECT_GE(t2, t1);
}

} // namespace
} // namespace solar_net
