#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

#include "timestamp.h"

namespace solar_net {

using TimerCallback = std::function<void()>;
/// 定时器，用于管理定时器回调
class Timer {
public:
    Timer(TimerCallback cb, Timestamp when, double interval);
    ~Timer() = default;
    /// 执行定时器回调
    void run() const { callback_(); }

    /// 获取定时器到期时间
    Timestamp expiration() const { return expiration_; }
    /// 获取定时器是否重复
    bool repeat() const { return interval_ > 0.0; }
    /// 获取定时器序列
    int64_t sequence() const { return sequence_; }
    /// 重启定时器

    void restart(Timestamp now);

private:
    TimerCallback callback_;
    Timestamp expiration_;
    const double interval_;
    const int64_t sequence_;

    static std::atomic<int64_t> s_num_created_;
};

/// 定时器 ID，用于标识定时器
class TimerId {
public:
    TimerId() : timer_(nullptr), sequence_(0) {}

    TimerId(Timer* timer, int64_t seq)
        : timer_(timer)
        , sequence_(seq) {}

    friend class TimerQueue;

private:
    Timer* timer_;
    int64_t sequence_;
};

} // namespace solar_net
