#pragma once

#include <memory>
#include <set>
#include <vector>

#include "timer.h"

namespace solar_net {

class Channel;
class EventLoop;

class TimerQueue {
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    TimerId add_timer(TimerCallback cb, Timestamp when, double interval);
    void cancel(TimerId timer_id);

    int get_next_timeout_ms() const;

private:
    using TimerEntry = std::pair<Timestamp, Timer*>;

    void handle_read();
    void add_timer_in_loop(Timer* timer);
    void cancel_in_loop(TimerId timer_id);
    void insert(Timer* timer);
    std::vector<Timer*> get_expired(Timestamp now);
    void reset_timerfd(Timestamp expiration);

    EventLoop* loop_;
    const int timerfd_;
    std::unique_ptr<Channel> timerfd_channel_;

    std::set<TimerEntry> timers_;
    bool calling_expired_timers_ = false;
    std::set<Timer*> canceling_timers_;
};

} // namespace solar_net
