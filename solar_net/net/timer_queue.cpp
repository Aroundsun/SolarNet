#include "solar_net/net/timer_queue.h"

#include "solar_net/net/channel.h"
#include "solar_net/net/event_loop.h"
#include "solar_net/base/logger.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <climits>
#include <cstring>
#include <format>

namespace solar_net {

namespace {

constexpr int kInvalidFd = -1;

int CreateTimerFd() {
    const int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        LOG_ERROR(std::format("timerfd_create() failed: {}", std::strerror(errno)));
    }
    return fd;
}

void SetTimerFd(int fd, Time expiration) {
    if (fd < 0) {
        return;
    }

    const Time now = Time::Now();
    auto delay = expiration.GetTimePoint() - now.GetTimePoint();
    if (delay < std::chrono::milliseconds::zero()) {
        delay = std::chrono::milliseconds::zero();
    }

    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(delay);
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(delay - seconds);

    struct itimerspec ts {};
    ts.it_value.tv_sec = seconds.count();
    ts.it_value.tv_nsec = nanoseconds.count();

    // A zero value would disarm the timer. If it is already expired, set it to
    // fire as soon as possible (1 nanosecond).
    if (ts.it_value.tv_sec == 0 && ts.it_value.tv_nsec == 0) {
        ts.it_value.tv_nsec = 1;
    }

    if (::timerfd_settime(fd, 0, &ts, nullptr) < 0) {
        LOG_ERROR(std::format("timerfd_settime() failed: {}", std::strerror(errno)));
    }
}

} // namespace

TimerQueue::TimerQueue(EventLoop* loop)
    : m_loop(loop)
    , m_timer_fd(CreateTimerFd()) {
    assert(loop != nullptr);

    if (m_timer_fd >= 0) {
        m_timer_channel = std::make_unique<Channel>(loop, m_timer_fd);
        m_timer_channel->SetReadCallback([this](Time) { HandleRead(); });
        m_timer_channel->EnableReading();
    }
}

TimerQueue::~TimerQueue() {
    if (m_timer_channel != nullptr) {
        m_timer_channel->DisableAll();
        m_loop->RemoveChannel(m_timer_channel.get());
    }

    if (m_timer_fd >= 0) {
        ::close(m_timer_fd);
    }
}

TimerQueue::TimerId TimerQueue::RunAt(Time time, TimerCallback callback) {
    auto entry = std::make_unique<TimerEntry>();
    entry->id = GenerateNextId();
    entry->expiration = time;
    entry->interval = std::chrono::milliseconds::zero();
    entry->callback = std::move(callback);

    const TimerId id = entry->id;
    m_active_timers.emplace(entry->expiration, id);
    m_timers[id] = std::move(entry);

    ResetTimerFd(m_active_timers.begin()->first);

    return id;
}

TimerQueue::TimerId TimerQueue::RunAfter(std::chrono::milliseconds delay, TimerCallback callback) {
    return RunAt(Time{Time::Now().GetTimePoint() + delay}, std::move(callback));
}

TimerQueue::TimerId TimerQueue::RunEvery(std::chrono::milliseconds interval, TimerCallback callback) {
    auto entry = std::make_unique<TimerEntry>();
    entry->id = GenerateNextId();
    entry->expiration = Time{Time::Now().GetTimePoint() + interval};
    entry->interval = interval;
    entry->callback = std::move(callback);

    const TimerId id = entry->id;
    m_active_timers.emplace(entry->expiration, id);
    m_timers[id] = std::move(entry);

    ResetTimerFd(m_active_timers.begin()->first);

    return id;
}

bool TimerQueue::Cancel(TimerId id) {
    const auto it = m_timers.find(id);
    if (it == m_timers.end()) {
        return false;
    }

    const Time expiration = it->second->expiration;
    m_active_timers.erase({expiration, id});
    m_timers.erase(it);

    if (!m_active_timers.empty()) {
        ResetTimerFd(m_active_timers.begin()->first);
    } else if (m_timer_fd >= 0) {
        struct itimerspec ts {};
        ::timerfd_settime(m_timer_fd, 0, &ts, nullptr);
    }

    return true;
}

int TimerQueue::NextTimeout() const {
    if (m_active_timers.empty()) {
        return -1;
    }

    const Time now = Time::Now();
    const auto delay = m_active_timers.begin()->first.GetTimePoint() - now.GetTimePoint();
    if (delay <= std::chrono::milliseconds::zero()) {
        return 0;
    }

    const auto delay_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delay).count();
    if (delay_ms > INT_MAX) {
        return INT_MAX;
    }

    return static_cast<int>(delay_ms);
}

void TimerQueue::HandleRead() {
    if (m_timer_fd < 0) {
        return;
    }

    uint64_t count = 0;
    const ssize_t n = ::read(m_timer_fd, &count, sizeof(count));
    if (n != sizeof(count)) {
        LOG_ERROR(std::format("TimerQueue::HandleRead() reads {} bytes, expected {}", n, sizeof(count)));
    }

    ProcessExpiredTimers(Time::Now());
}

void TimerQueue::ResetTimerFd(Time expiration) {
    SetTimerFd(m_timer_fd, expiration);
}

void TimerQueue::ProcessExpiredTimers(Time now) {
    while (!m_active_timers.empty()) {
        const auto it = m_active_timers.begin();
        if (it->first > now) {
            break;
        }

        const TimerId id = it->second;
        m_active_timers.erase(it);

        const auto timer_it = m_timers.find(id);
        if (timer_it == m_timers.end()) {
            continue;
        }

        TimerEntry* entry = timer_it->second.get();
        if (entry->callback) {
            entry->callback();
        }

        if (entry->interval > std::chrono::milliseconds::zero()) {
            entry->expiration = Time{entry->expiration.GetTimePoint() + entry->interval};
            m_active_timers.emplace(entry->expiration, id);
        } else {
            m_timers.erase(timer_it);
        }
    }

    if (!m_active_timers.empty()) {
        ResetTimerFd(m_active_timers.begin()->first);
    } else if (m_timer_fd >= 0) {
        struct itimerspec ts {};
        ::timerfd_settime(m_timer_fd, 0, &ts, nullptr);
    }
}

TimerQueue::TimerId TimerQueue::GenerateNextId() {
    return m_next_id++;
}

} // namespace solar_net
