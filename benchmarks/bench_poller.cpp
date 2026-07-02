#include "solar_net/net/epoll_poller.h"

#include "solar_net/net/event_loop.h"

#include <benchmark/benchmark.h>

#include <vector>

static void EpollPoller_PollEmpty(benchmark::State& state) {
    solar_net::EventLoop loop;
    auto& poller = static_cast<solar_net::EpollPoller&>(loop.GetPoller());

    for (auto _ : state) {
        std::vector<solar_net::Channel*> active;
        poller.Poll(0, &active);
    }
}

BENCHMARK(EpollPoller_PollEmpty);

BENCHMARK_MAIN();
