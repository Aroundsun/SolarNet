#pragma once

#include "solar_net/base/non_copyable.h"
#include "solar_net/base/time.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>

namespace solar_net {

class Channel;
class EventLoop;

/**
 * @brief 基于 Linux timerfd 的定时器队列，服务于 EventLoop。
 *
 * 用有序 set 维护到期时间，timerfd 在最早定时器到期时唤醒 loop。
 * 线程安全：否，仅能在 loop 线程使用。
 */
class TimerQueue : NonCopyable {
public:
    using TimerCallback = std::function<void()>;
    using TimerId = int64_t;

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    /** @brief 在绝对时间点执行一次。线程安全：否。 */
    TimerId RunAt(Time time, TimerCallback callback);
    /** @brief 延迟 delay 后执行一次。线程安全：否。 */
    TimerId RunAfter(std::chrono::milliseconds delay, TimerCallback callback);
    /** @brief 每隔 interval 重复执行。线程安全：否。 */
    TimerId RunEvery(std::chrono::milliseconds interval, TimerCallback callback);
    /** @brief 取消定时器，存在且取消成功返回 true。线程安全：否。 */
    bool Cancel(TimerId id);

    /** @brief 距下一个定时器的毫秒数；-1 表示无定时器。线程安全：否。 */
    [[nodiscard]] int NextTimeout() const;

private:
    struct TimerEntry {
        TimerId id;
        Time expiration;
        std::chrono::milliseconds interval;
        TimerCallback callback;
    };

    using ActiveTimerSet = std::set<std::pair<Time, TimerId>>;
    using TimerMap = std::unordered_map<TimerId, std::unique_ptr<TimerEntry>>;

    void HandleRead();
    void ResetTimerFd(Time expiration);
    void ProcessExpiredTimers(Time now);

    TimerId GenerateNextId();

    EventLoop* m_loop;
    int m_timer_fd{-1};
    std::unique_ptr<Channel> m_timer_channel;

    TimerMap m_timers;
    ActiveTimerSet m_active_timers;
    TimerId m_next_id{1};
};

} // namespace solar_net
