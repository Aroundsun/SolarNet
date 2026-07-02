#include "solar_net/net/transport/inet_address.h"

#include <benchmark/benchmark.h>

namespace solar_net {

static void BM_InetAddressToIp(benchmark::State& state) {
    const InetAddress addr("192.168.1.1", 8080);
    for (auto _ : state) {
        benchmark::DoNotOptimize(addr.ToIp());
    }
}
BENCHMARK(BM_InetAddressToIp);

static void BM_InetAddressToIpPort(benchmark::State& state) {
    const InetAddress addr("192.168.1.1", 8080);
    for (auto _ : state) {
        benchmark::DoNotOptimize(addr.ToIpPort());
    }
}
BENCHMARK(BM_InetAddressToIpPort);

static void BM_InetAddressConstructFromIpPort(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(InetAddress("192.168.1.1", 8080));
    }
}
BENCHMARK(BM_InetAddressConstructFromIpPort);

} // namespace solar_net

BENCHMARK_MAIN();
