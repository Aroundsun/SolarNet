#include "solar_net/base/time.h"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace solar_net {

Time Time::Now() noexcept {
    return Time{Clock::now()};
}

Time::Time(TimePoint tp) noexcept : m_time_point(tp) {}

Time::TimePoint Time::GetTimePoint() const noexcept {
    return m_time_point;
}

std::chrono::milliseconds Time::MillisecondsSinceEpoch() const noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(m_time_point.time_since_epoch());
}

std::string Time::ToIso8601() const {
    return ToFormattedString("%Y-%m-%dT%H:%M:%S");
}

std::string Time::ToFormattedString(const std::string& format) const {
    const auto time_t = Clock::to_time_t(m_time_point);
    std::tm tm{};
    std::tm* result = localtime_r(&time_t, &tm);
    if (result == nullptr) {
        return {};
    }

    std::ostringstream oss;
    oss << std::put_time(&tm, format.c_str());
    return oss.str();
}

std::string Time::ToLogString() const {
    const auto time_t = Clock::to_time_t(m_time_point);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(m_time_point.time_since_epoch()).count() % 1000;

    std::tm tm{};
    std::tm* result = localtime_r(&time_t, &tm);
    if (result == nullptr) {
        return {};
    }

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms;
    return oss.str();
}

} // namespace solar_net
