#include "solar_net/base/buffer.h"

#include <benchmark/benchmark.h>

namespace solar_net {

static void BM_BufferAppendSmall(benchmark::State& state) {
    Buffer buffer;
    const std::string data = "hello world";
    for (auto _ : state) {
        buffer.Append(data);
        benchmark::DoNotOptimize(buffer);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(data.size()));
}
BENCHMARK(BM_BufferAppendSmall);

static void BM_BufferAppendLarge(benchmark::State& state) {
    Buffer buffer;
    const std::string data(static_cast<size_t>(state.range(0)), 'a');
    for (auto _ : state) {
        buffer.Append(data);
        benchmark::DoNotOptimize(buffer);
    }
    state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) * static_cast<int64_t>(data.size()));
}
BENCHMARK(BM_BufferAppendLarge)->Arg(1024)->Arg(4096)->Arg(65536);

static void BM_BufferReadInt32(benchmark::State& state) {
    Buffer buffer;
    for (int i = 0; i < 10000; ++i) {
        buffer.AppendInt32(i);
    }
    for (auto _ : state) {
        Buffer copy = buffer;
        for (int i = 0; i < 10000; ++i) {
            benchmark::DoNotOptimize(copy.ReadInt32());
        }
    }
}
BENCHMARK(BM_BufferReadInt32);

static void BM_BufferFindCRLF(benchmark::State& state) {
    Buffer buffer;
    const std::string line = "GET / HTTP/1.1\r\n";
    for (int i = 0; i < 1000; ++i) {
        buffer.Append(line);
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(buffer.FindCRLF());
    }
}
BENCHMARK(BM_BufferFindCRLF);

} // namespace solar_net

BENCHMARK_MAIN();
