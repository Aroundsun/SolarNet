#include "solar_net/net/event_loop_thread_pool.h"

#include <benchmark/benchmark.h>

static void EventLoopThreadPool_StartStop(benchmark::State& state) {
    for (auto _ : state) {
        solar_net::EventLoopThreadPool pool(4, "bench");
        pool.Start();
        pool.Stop();
    }
}

BENCHMARK(EventLoopThreadPool_StartStop);

static void EventLoopThreadPool_GetNextLoop(benchmark::State& state) {
    solar_net::EventLoopThreadPool pool(4, "bench");
    pool.Start();

    for (auto _ : state) {
        benchmark::DoNotOptimize(pool.GetNextLoop());
    }

    pool.Stop();
}

BENCHMARK(EventLoopThreadPool_GetNextLoop);

BENCHMARK_MAIN();
