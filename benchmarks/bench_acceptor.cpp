#include "solar_net/net/transport/acceptor.h"

#include <benchmark/benchmark.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "solar_net/net/event_loop.h"
#include "solar_net/net/event_loop_thread.h"
#include "solar_net/net/transport/inet_address.h"

namespace solar_net {

static void BM_AcceptorListen(benchmark::State& state) {
    for (auto _ : state) {
        EventLoop loop;
        Acceptor acceptor(&loop, InetAddress(0));
        benchmark::DoNotOptimize(acceptor.Listen());
    }
}
BENCHMARK(BM_AcceptorListen);

static void BM_AcceptorAcceptOne(benchmark::State& state) {
    constexpr uint16_t kPort = 30999;

    EventLoopThread loop_thread("acceptor-bench");
    EventLoop* loop = loop_thread.Start();
    if (loop == nullptr) {
        state.SkipWithError("failed to start loop thread");
        return;
    }

    std::atomic<int> count{0};
    std::shared_ptr<Acceptor> acceptor;

    loop->RunInLoop([&] {
        acceptor = std::make_shared<Acceptor>(loop, InetAddress(kPort));
        acceptor->SetNewConnectionCallback([&count](int fd, const InetAddress&) {
            ++count;
            ::close(fd);
        });
        acceptor->Listen();
    });

    for (auto _ : state) {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            continue;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kPort);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            while (count.load(std::memory_order_relaxed) == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            count.fetch_sub(1, std::memory_order_relaxed);
        }
        ::close(fd);
    }

    loop_thread.Stop();
}
BENCHMARK(BM_AcceptorAcceptOne);

} // namespace solar_net

BENCHMARK_MAIN();
