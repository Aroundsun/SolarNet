#include "solar_net/net/event_loop_thread.h"

#include <benchmark/benchmark.h>

static void EventLoopThread_StartStop(benchmark::State& state) {
    for (auto _ : state) {
        solar_net::EventLoopThread loop_thread("bench");
        loop_thread.Start();
        loop_thread.Stop();
    }
}

BENCHMARK(EventLoopThread_StartStop);

BENCHMARK_MAIN();
