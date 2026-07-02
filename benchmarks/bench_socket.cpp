#include "solar_net/net/transport/socket.h"

#include <benchmark/benchmark.h>

#include "solar_net/net/transport/inet_address.h"

namespace solar_net {

static void BM_SocketCreateClose(benchmark::State& state) {
    for (auto _ : state) {
        Socket socket = Socket::CreateTcp();
        benchmark::DoNotOptimize(socket.Fd());
        socket.Close();
    }
}
BENCHMARK(BM_SocketCreateClose);

static void BM_SocketBindListen(benchmark::State& state) {
    for (auto _ : state) {
        Socket socket = Socket::CreateTcp();
        socket.SetReuseAddr(true);
        benchmark::DoNotOptimize(socket.Bind(InetAddress(0)));
        benchmark::DoNotOptimize(socket.Listen());
        socket.Close();
    }
}
BENCHMARK(BM_SocketBindListen);

static void BM_SocketAcceptEmpty(benchmark::State& state) {
    Socket socket = Socket::CreateTcp();
    socket.SetNonBlocking(true);
    socket.Bind(InetAddress(0));
    socket.Listen();

    for (auto _ : state) {
        auto [fd, peer] = socket.Accept();
        benchmark::DoNotOptimize(fd);
        benchmark::DoNotOptimize(peer.ToIpPort());
    }
}
BENCHMARK(BM_SocketAcceptEmpty);

} // namespace solar_net

BENCHMARK_MAIN();
