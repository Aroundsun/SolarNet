#include "solar_net/version.h"

#include <benchmark/benchmark.h>

namespace solar_net {

static void BM_VersionString(benchmark::State& state) {
    for (auto _ : state) {
        auto version = Version::VersionString();
        benchmark::DoNotOptimize(version);
    }
}
BENCHMARK(BM_VersionString);

static void BM_ProjectName(benchmark::State& state) {
    for (auto _ : state) {
        auto name = Version::ProjectName();
        benchmark::DoNotOptimize(name);
    }
}
BENCHMARK(BM_ProjectName);

} // namespace solar_net

BENCHMARK_MAIN();
