#include "solar_net/net/epoll_poller.h"

#include "solar_net/net/channel.h"
#include "solar_net/net/event_loop.h"
#include "solar_net/base/logger.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <format>

#include <sys/epoll.h>
#include <unistd.h>

namespace solar_net {

namespace {

constexpr int kNew = -1;
constexpr int kAdded = 1;
constexpr int kDeleted = 2;

} // namespace

EpollPoller::EpollPoller(EventLoop* loop) : Poller(loop), m_events(kInitEventListSize) {
    m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (m_epoll_fd < 0) {
        LOG_FATAL(std::format("EpollPoller::EpollPoller epoll_create1 failed: {}", std::strerror(errno)));
    }
}

EpollPoller::~EpollPoller() {
    if (m_epoll_fd >= 0) {
        ::close(m_epoll_fd);
        m_epoll_fd = -1;
    }
}

Time EpollPoller::Poll(int timeout_ms, ChannelList* active_channels) {
    assert(active_channels != nullptr);

    const int num_events = epoll_wait(m_epoll_fd, m_events.data(),
                                      static_cast<int>(m_events.size()), timeout_ms);
    if (num_events < 0 && errno != EINTR) {
        LOG_ERROR(std::format("EpollPoller::Poll epoll_wait failed: {}", std::strerror(errno)));
    }

    const Time now = Time::Now();
    active_channels->clear();

    if (num_events > 0) {
        active_channels->reserve(static_cast<size_t>(num_events));
        for (int i = 0; i < num_events; ++i) {
            auto* channel = static_cast<Channel*>(m_events[static_cast<size_t>(i)].data.ptr);
            channel->SetRevents(static_cast<int>(m_events[static_cast<size_t>(i)].events));
            active_channels->push_back(channel);
        }

        if (static_cast<size_t>(num_events) == m_events.size()) {
            m_events.resize(m_events.size() * 2);
        }
    }

    return now;
}

void EpollPoller::UpdateChannel(Channel* channel) {
    AssertInLoopThread();
    assert(channel != nullptr);

    const int index = channel->Index();
    LOG_DEBUG(std::format("EpollPoller::UpdateChannel fd={} events={} index={}",
                          channel->Fd(), channel->Events(), index));

    if (index == kNew || index == kDeleted) {
        const int fd = channel->Fd();
        if (index == kNew) {
            assert(m_channels.find(fd) == m_channels.end());
            m_channels[fd] = channel;
        }

        if (!channel->IsNoneEvent()) {
            Update(EPOLL_CTL_ADD, channel);
            channel->SetIndex(kAdded);
        }
    } else {
        assert(index == kAdded);
        const int fd = channel->Fd();
        if (channel->IsNoneEvent()) {
            Update(EPOLL_CTL_DEL, channel);
            channel->SetIndex(kDeleted);
        } else {
            Update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EpollPoller::RemoveChannel(Channel* channel) {
    AssertInLoopThread();
    assert(channel != nullptr);
    assert(channel->IsNoneEvent());

    const int index = channel->Index();
    LOG_DEBUG(std::format("EpollPoller::RemoveChannel fd={} index={}", channel->Fd(), index));

    const size_t count = m_channels.erase(channel->Fd());
    assert(count == 1);

    if (index == kAdded) {
        Update(EPOLL_CTL_DEL, channel);
    }
    channel->SetIndex(kNew);
}

void EpollPoller::Update(int operation, Channel* channel) {
    struct epoll_event event {};
    event.events = static_cast<uint32_t>(channel->Events());
    event.data.ptr = channel;

    const int fd = channel->Fd();
    LOG_DEBUG(std::format("EpollPoller::Update {} fd={} events={}",
                          OperationToString(operation), fd, channel->Events()));

    if (::epoll_ctl(m_epoll_fd, operation, fd, &event) < 0) {
        LOG_ERROR(std::format("EpollPoller::Update epoll_ctl op={} fd={}: {}",
                              OperationToString(operation), fd, std::strerror(errno)));
    }
}

const char* EpollPoller::OperationToString(int op) {
    switch (op) {
    case EPOLL_CTL_ADD: return "ADD";
    case EPOLL_CTL_DEL: return "DEL";
    case EPOLL_CTL_MOD: return "MOD";
    default: return "Unknown";
    }
}

} // namespace solar_net
