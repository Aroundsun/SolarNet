#include "solar_net/net/event_loop.h"

#include "solar_net/net/channel.h"
#include "solar_net/net/poller.h"
#include "solar_net/net/timer_queue.h"
#include "solar_net/base/logger.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <format>
#include <utility>

#include <sys/eventfd.h>
#include <unistd.h>

namespace solar_net {

namespace {

constexpr int kPollTimeoutMs = 10000;

} // namespace

EventLoop::EventLoop()
    : m_thread_id(std::this_thread::get_id()), m_poller(Poller::NewDefaultPoller(this)) {
    m_wakeup_fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_wakeup_fd < 0) {
        LOG_FATAL(std::format("EventLoop::EventLoop eventfd failed: {}", std::strerror(errno)));
    }

    m_wakeup_channel = std::make_unique<Channel>(this, m_wakeup_fd);
    m_wakeup_channel->SetReadCallback([this](Time) { HandleRead(); });
    m_wakeup_channel->EnableReading();

    m_timer_queue = std::make_unique<TimerQueue>(this);
}

EventLoop::~EventLoop() {
    m_wakeup_channel->DisableAll();
    m_wakeup_channel->Remove();
    m_wakeup_channel.reset();

    if (m_wakeup_fd >= 0) {
        ::close(m_wakeup_fd);
        m_wakeup_fd = -1;
    }
}

void EventLoop::Loop() {
    AssertInLoopThread();
    assert(!m_looping.exchange(true));

    while (true) {
        if (!m_quit.load(std::memory_order_acquire)) {
            Poller::ChannelList active_channels;
            const int poll_timeout_ms = NextTimeout();
            const Time poll_return_time = m_poller->Poll(poll_timeout_ms, &active_channels);
            m_event_handling.store(true, std::memory_order_release);

            for (Channel* channel : active_channels) {
                channel->HandleEvent(poll_return_time);
            }
            m_event_handling.store(false, std::memory_order_release);
        }

        DoPendingFunctors();

        if (m_quit.load(std::memory_order_acquire)) {
            break;
        }
    }

    m_looping.store(false, std::memory_order_release);
}

void EventLoop::Quit() {
    m_quit.store(true, std::memory_order_release);
    if (!IsInLoopThread()) {
        Wakeup();
    }
}

bool EventLoop::IsInLoopThread() const noexcept {
    return std::this_thread::get_id() == m_thread_id;
}

void EventLoop::AssertInLoopThread() const {
    if (!IsInLoopThread()) {
        LOG_FATAL("EventLoop::AssertInLoopThread - not in loop thread");
    }
}

void EventLoop::RunInLoop(Functor cb) {
    if (IsInLoopThread()) {
        cb();
    } else {
        QueueInLoop(std::move(cb));
    }
}

void EventLoop::QueueInLoop(Functor cb) {
    {
        std::lock_guard lock(m_mutex);
        m_pending_functors.push_back(std::move(cb));
    }

    if (!IsInLoopThread() || m_calling_pending_functors.load(std::memory_order_acquire)) {
        Wakeup();
    }
}

void EventLoop::UpdateChannel(Channel* channel) {
    AssertInLoopThread();
    m_poller->UpdateChannel(channel);
}

void EventLoop::RemoveChannel(Channel* channel) {
    AssertInLoopThread();
    m_poller->RemoveChannel(channel);
}

void EventLoop::Wakeup() {
    uint64_t one = 1;
    const ssize_t n = ::write(m_wakeup_fd, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR(std::format("EventLoop::Wakeup write {} bytes instead of 8", n));
    }
}

void EventLoop::HandleRead() {
    uint64_t one = 0;
    const ssize_t n = ::read(m_wakeup_fd, &one, sizeof(one));
    if (n != sizeof(one)) {
        LOG_ERROR(std::format("EventLoop::HandleRead read {} bytes instead of 8", n));
    }
}

void EventLoop::DoPendingFunctors() {
    std::vector<Functor> functors;
    m_calling_pending_functors.store(true, std::memory_order_release);

    {
        std::lock_guard lock(m_mutex);
        functors.swap(m_pending_functors);
    }

    for (auto& functor : functors) {
        functor();
    }

    m_calling_pending_functors.store(false, std::memory_order_release);
}

TimerQueue::TimerId EventLoop::RunAt(Time time, TimerQueue::TimerCallback cb) {
    AssertInLoopThread();
    return m_timer_queue->RunAt(time, std::move(cb));
}

TimerQueue::TimerId EventLoop::RunAfter(std::chrono::milliseconds delay, TimerQueue::TimerCallback cb) {
    AssertInLoopThread();
    return m_timer_queue->RunAfter(delay, std::move(cb));
}

TimerQueue::TimerId EventLoop::RunEvery(std::chrono::milliseconds interval, TimerQueue::TimerCallback cb) {
    AssertInLoopThread();
    return m_timer_queue->RunEvery(interval, std::move(cb));
}

bool EventLoop::Cancel(TimerQueue::TimerId id) {
    AssertInLoopThread();
    return m_timer_queue->Cancel(id);
}

int EventLoop::NextTimeout() const {
    return m_timer_queue ? m_timer_queue->NextTimeout() : -1;
}

} // namespace solar_net
