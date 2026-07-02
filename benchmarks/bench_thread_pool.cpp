#include "solar_net/base/thread_pool.h"

#include <benchmark/benchmark.h>

#include <atomic>
#include <thread>

static void ThreadPool_SubmitThroughput(benchmark::State& state) {
    const size_t thread_count = static_cast<size_t>(state.range(0));
    solar_net::ThreadPool pool(thread_count);
    pool.Start();

    std::atomic<uint64_t> counter{0};
    const auto task = [&counter] { counter.fetch_add(1, std::memory_order_relaxed); };

    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            pool.Submit(task);
        }

        // Drain all tasks before the next iteration.
        while (pool.PendingTaskCount() > 0) {
            std::this_thread::yield();
        }
    }

    pool.Stop();
    pool.Wait();

    state.SetItemsProcessed(static_cast<int64_t>(counter.load()));
}

BENCHMARK(ThreadPool_SubmitThroughput)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

BENCHMARK_MAIN();
