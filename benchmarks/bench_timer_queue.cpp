#include "solar_net/net/event_loop.h"

#include <benchmark/benchmark.h>

#include <chrono>

static void TimerQueue_RunAfterAndCancel(benchmark::State& state) {
    solar_net::EventLoop loop;

    for (auto _ : state) {
        const auto id = loop.RunAfter(std::chrono::milliseconds(1000), [] {});
        benchmark::DoNotOptimize(id);
        loop.Cancel(id);
    }
}

BENCHMARK(TimerQueue_RunAfterAndCancel);

BENCHMARK_MAIN();
