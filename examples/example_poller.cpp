#include "solar_net/net/epoll_poller.h"

#include "solar_net/net/channel.h"
#include "solar_net/net/event_loop.h"

#include <format>
#include <iostream>

#include <unistd.h>

int main() {
    solar_net::EventLoop loop;
    auto& poller = static_cast<solar_net::EpollPoller&>(loop.GetPoller());

    int fds[2];
    if (pipe(fds) < 0) {
        return 1;
    }

    solar_net::Channel channel(&loop, fds[0]);
    int read_count = 0;
    channel.SetReadCallback([&](solar_net::Time) { ++read_count; });
    channel.EnableReading();

    const char c = 'x';
    (void)::write(fds[1], &c, sizeof(c));

    solar_net::Poller::ChannelList active;
    poller.Poll(0, &active);

    for (auto* ch : active) {
        ch->HandleEvent(solar_net::Time::Now());
    }

    std::cout << std::format("{} events, read_count={}\n", active.size(), read_count);

    channel.DisableAll();
    poller.RemoveChannel(&channel);
    close(fds[0]);
    close(fds[1]);

    return 0;
}
