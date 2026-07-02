#include "solar_net/net/event_loop.h"

#include <benchmark/benchmark.h>

#include <atomic>
#include <thread>

static void EventLoop_RunInLoop(benchmark::State& state) {
    solar_net::EventLoop loop;

    for (auto _ : state) {
        loop.RunInLoop([] {});
    }
}

BENCHMARK(EventLoop_RunInLoop);

static void EventLoop_QueueAndQuit(benchmark::State& state) {
    for (auto _ : state) {
        solar_net::EventLoop loop;
        std::atomic<bool> task_run{false};

        std::thread worker([&] {
            loop.QueueInLoop([&] {
                task_run.store(true, std::memory_order_relaxed);
                loop.Quit();
            });
        });

        loop.Loop();
        worker.join();

        benchmark::DoNotOptimize(task_run.load());
    }
}

BENCHMARK(EventLoop_QueueAndQuit);

BENCHMARK_MAIN();
