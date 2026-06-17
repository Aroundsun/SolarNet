#include "event_loop.h"
#include "epoll_poller.h"
#include "channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <thread>
#include <algorithm>

namespace solar_net {

// 线程本地存储，用于当前线程的事件循环
thread_local EventLoop* t_loop_in_this_thread = nullptr;

EventLoop::EventLoop()
    : looping_(false)
    , stop_(false)
    , thread_id_(std::this_thread::get_id())
    , poller_(std::make_unique<EpollPoller>(this))
    , wakeup_fd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))
    , wakeup_channel_(nullptr) {

    if (t_loop_in_this_thread != nullptr) {
        // 当前线程已经有事件循环
        ::abort();
    }
    t_loop_in_this_thread = this;

    if (wakeup_fd_ < 0) {
        ::abort();
    }

    // 设置唤醒通道
    wakeup_channel_ = std::make_unique<Channel>(this, wakeup_fd_);
    wakeup_channel_->set_read_callback([this]() { handle_read(); });
    wakeup_channel_->enable_reading();
}

EventLoop::~EventLoop() {
    wakeup_channel_->disable_all();
    wakeup_channel_->remove();
    close_wakeup_fd();
    t_loop_in_this_thread = nullptr;
}

void EventLoop::loop() {
    assert(!looping_);
    assert(is_in_loop_thread());
    looping_ = true;
    stop_ = false;

    while (!stop_) {
        active_channels_.clear();
        poller_->poll(kPollTimeoutMs, &active_channels_);

        for (auto* channel : active_channels_) {
            channel->handle_event();
        }

        do_pending_tasks();
    }

    looping_ = false;
}

void EventLoop::stop() {
    stop_ = true;
    if (!is_in_loop_thread()) {
        wakeup();
    }
}

void EventLoop::run_in_loop(Task task) {
    if (is_in_loop_thread()) {
        task();
    } else {
        queue_in_loop(std::move(task));
    }
}

void EventLoop::queue_in_loop(Task task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_tasks_.push_back(std::move(task));
    }

    if (!is_in_loop_thread()) {
        wakeup();
    }
}

void EventLoop::assert_in_loop_thread() {
    if (!is_in_loop_thread()) {
        ::abort();
    }
}

bool EventLoop::is_in_loop_thread() const {
    return std::this_thread::get_id() == thread_id_;
}

void EventLoop::update_channel(Channel* channel) {
    assert(channel->owner_loop() == this);
    assert_in_loop_thread();
    poller_->update_channel(channel);
}

void EventLoop::remove_channel(Channel* channel) {
    assert(channel->owner_loop() == this);
    assert_in_loop_thread();
    poller_->remove_channel(channel);
}

EventLoop* EventLoop::get_event_loop_of_current_thread() {
    return t_loop_in_this_thread;
}

void EventLoop::do_pending_tasks() {
    std::vector<Task> tasks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks.swap(pending_tasks_);
    }

    for (auto& task : tasks) {
        task();
    }
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeup_fd_, &one, sizeof(one));
    (void)n;
}

void EventLoop::handle_read() {
    uint64_t one = 0;
    ssize_t n = ::read(wakeup_fd_, &one, sizeof(one));
    (void)n;
}

void EventLoop::close_wakeup_fd() {
    if (wakeup_fd_ >= 0) {
        ::close(wakeup_fd_);
        wakeup_fd_ = -1;
    }
}

} // namespace solar_net
