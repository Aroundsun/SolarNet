#include "solar_net/net/channel.h"

#include "solar_net/net/event_loop.h"
#include "solar_net/base/time.h"

#include <format>
#include <iostream>

#include <poll.h>

int main() {
    solar_net::EventLoop loop;
    solar_net::Channel channel(&loop, 0);

    int read_count = 0;
    int write_count = 0;
    int close_count = 0;
    int error_count = 0;

    channel.SetReadCallback([&](solar_net::Time t) {
        (void)t;
        ++read_count;
    });
    channel.SetWriteCallback([&] { ++write_count; });
    channel.SetCloseCallback([&] { ++close_count; });
    channel.SetErrorCallback([&] { ++error_count; });

    channel.EnableReading();
    channel.EnableWriting();

    channel.SetRevents(POLLIN | POLLOUT);
    channel.HandleEvent(solar_net::Time::Now());

    channel.SetRevents(POLLHUP);
    channel.HandleEvent(solar_net::Time::Now());

    channel.SetRevents(POLLERR);
    channel.HandleEvent(solar_net::Time::Now());

    std::cout << std::format(
        "read={} write={} close={} error={}\n", read_count, write_count, close_count, error_count);

    channel.DisableAll();
    return 0;
}
