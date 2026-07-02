#include "solar_net/net/poller.h"

#include "solar_net/net/channel.h"
#include "solar_net/net/epoll_poller.h"
#include "solar_net/net/event_loop.h"

#include <cassert>

namespace solar_net {

Poller::Poller(EventLoop* loop) : m_loop(loop) {
    assert(loop != nullptr);
}

Poller::~Poller() = default;

bool Poller::HasChannel(Channel* channel) const {
    AssertInLoopThread();
    if (channel == nullptr) {
        return false;
    }
    const auto it = m_channels.find(channel->Fd());
    return it != m_channels.end() && it->second == channel;
}

std::unique_ptr<Poller> Poller::NewDefaultPoller(EventLoop* loop) {
    return std::make_unique<EpollPoller>(loop);
}

void Poller::AssertInLoopThread() const {
    if (m_loop != nullptr) {
        m_loop->AssertInLoopThread();
    }
}

} // namespace solar_net
