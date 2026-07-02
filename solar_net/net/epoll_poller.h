#pragma once

#include "solar_net/net/poller.h"

#include <sys/epoll.h>
#include <vector>

namespace solar_net {

class Channel;
class EventLoop;

/**
 * @brief Linux epoll 实现的 IO 多路复用。
 *
 * 线程安全：否，须在所属 EventLoop 线程调用。
 */
class EpollPoller : public Poller {
public:
    explicit EpollPoller(EventLoop* loop);
    ~EpollPoller() override;

    Time Poll(int timeout_ms, ChannelList* active_channels) override;
    void UpdateChannel(Channel* channel) override;
    void RemoveChannel(Channel* channel) override;

private:
    static constexpr int kInitEventListSize = 16;

    static const char* OperationToString(int op);

    void Update(int operation, Channel* channel);

    int m_epoll_fd{-1};
    std::vector<struct epoll_event> m_events;
};

} // namespace solar_net
