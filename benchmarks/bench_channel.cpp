#include "solar_net/net/channel.h"

#include "solar_net/net/event_loop.h"
#include "solar_net/base/time.h"

#include <benchmark/benchmark.h>

#include <poll.h>

static void Channel_HandleEventDispatch(benchmark::State& state) {
    solar_net::EventLoop loop;
    solar_net::Channel channel(&loop, 0);

    int counter = 0;
    channel.SetReadCallback([&counter](solar_net::Time t) {
        (void)t;
        ++counter;
    });
    channel.SetWriteCallback([&counter] { ++counter; });
    channel.SetCloseCallback([&counter] { ++counter; });
    channel.SetErrorCallback([&counter] { ++counter; });

    channel.SetRevents(POLLIN | POLLOUT | POLLHUP | POLLERR);

    for (auto _ : state) {
        channel.HandleEvent(solar_net::Time::Now());
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

BENCHMARK(Channel_HandleEventDispatch);

BENCHMARK_MAIN();
