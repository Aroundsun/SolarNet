#pragma once

#include <chrono>
#include <compare>
#include <string>

namespace solar_net {

/**
 * @brief 基于 system_clock 的时间戳值类型，提供格式化与比较。
 *
 * 线程安全：只读访问同一 const Time 对象可并发；各方法无共享可变状态。
 */
class Time {
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = Clock::time_point;

    /** @brief 返回当前系统时间。线程安全。 */
    [[nodiscard]] static Time Now() noexcept;

    /** @brief 默认构造，表示最小时间点。线程安全。 */
    Time() = default;

    /** @brief 从时间点构造。线程安全。 */
    explicit Time(TimePoint tp) noexcept;

    /** @brief 返回内部时间点。线程安全。 */
    [[nodiscard]] TimePoint GetTimePoint() const noexcept;

    /** @brief 返回自 Unix 纪元起的毫秒数。线程安全。 */
    [[nodiscard]] std::chrono::milliseconds MillisecondsSinceEpoch() const noexcept;

    /** @brief 格式化为 ISO 8601 字符串（本地时区）。线程安全。 */
    [[nodiscard]] std::string ToIso8601() const;

    /**
     * @brief 按 strftime 格式串格式化（本地时区）。
     * @param format 默认为 "%Y-%m-%d %H:%M:%S"。线程安全。
     */
    [[nodiscard]] std::string ToFormattedString(const std::string& format = "%Y-%m-%d %H:%M:%S") const;

    /** @brief 格式化为日志用时间串 "YYYY-MM-DD HH:MM:SS.mmm"。线程安全。 */
    [[nodiscard]] std::string ToLogString() const;

    /** @brief 三路比较运算符。线程安全。 */
    [[nodiscard]] std::strong_ordering operator<=>(const Time&) const noexcept = default;

private:
    TimePoint m_time_point{TimePoint::min()};
};

} // namespace solar_net
