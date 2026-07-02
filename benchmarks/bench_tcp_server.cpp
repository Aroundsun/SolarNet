#include "solar_net/net/transport/tcp_server.h"

#include "solar_net/base/buffer.h"
#include "solar_net/net/event_loop_thread.h"
#include "solar_net/net/transport/inet_address.h"

#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

namespace solar_net {

namespace {

class TcpServerFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State& state) override {
        m_loop_thread = std::make_unique<EventLoopThread>("bench-tcp-server");
        m_loop = m_loop_thread->Start();
        if (m_loop == nullptr) {
            state.SkipWithError("EventLoopThread start failed");
            return;
        }

        m_server = std::make_unique<TcpServer>(m_loop, InetAddress(0), "bench-server");
        m_server->SetThreadNum(0);
        m_server->SetMessageCallback([this](const TcpConnection::TcpConnectionPtr& conn, Buffer* buf, Time) {
            m_message_count.fetch_add(1, std::memory_order_relaxed);
            conn->Send(buf);
        });

        m_server->Start();

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (m_server->ListenAddress().Port() == 0 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (m_server->ListenAddress().Port() == 0) {
            state.SkipWithError("server failed to listen");
            return;
        }

        m_listen_addr = m_server->ListenAddress();

        const int clients = state.range(1);
        for (int i = 0; i < clients; ++i) {
            const int fd = Connect();
            if (fd < 0) {
                state.SkipWithError("connect failed");
                return;
            }
            m_client_fds.push_back(fd);
        }
    }

    void TearDown(benchmark::State& state) override {
        (void)state;
        for (int fd : m_client_fds) {
            ::close(fd);
        }
        m_client_fds.clear();
        if (m_server) {
            m_server->Stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            m_server.reset();
        }
        if (m_loop_thread) {
            m_loop_thread->Stop();
        }
    }

    [[nodiscard]] int Connect() const {
        const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            return -1;
        }
        if (::connect(fd, m_listen_addr.GetSockAddr(), m_listen_addr.GetSockLen()) != 0) {
            ::close(fd);
            return -1;
        }
        return fd;
    }

    EventLoop* m_loop{nullptr};
    std::unique_ptr<EventLoopThread> m_loop_thread;
    std::unique_ptr<TcpServer> m_server;
    InetAddress m_listen_addr;
    std::vector<int> m_client_fds;
    std::atomic<int> m_message_count{0};
};

} // namespace

BENCHMARK_DEFINE_F(TcpServerFixture, EchoRoundTrip)(benchmark::State& state) {
    const std::string payload(static_cast<size_t>(state.range(0)), 'x');
    char buf[8192]{};

    for (auto _ : state) {
        for (int fd : m_client_fds) {
            if (::write(fd, payload.data(), payload.size()) != static_cast<ssize_t>(payload.size())) {
                state.SkipWithError("write failed");
                return;
            }
        }

        for (int fd : m_client_fds) {
            size_t total = 0;
            while (total < payload.size()) {
                const ssize_t n = ::read(fd, buf + total, sizeof(buf) - total);
                if (n > 0) {
                    total += static_cast<size_t>(n);
                } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    break;
                } else {
                    std::this_thread::yield();
                }
            }
        }
    }

    state.SetBytesProcessed(static_cast<int64_t>(payload.size()) * state.iterations() *
                            static_cast<int64_t>(m_client_fds.size()));
}
BENCHMARK_REGISTER_F(TcpServerFixture, EchoRoundTrip)
    ->ArgsProduct({{64, 256, 1024}, {1, 10}});

BENCHMARK_DEFINE_F(TcpServerFixture, ConnectionEstablishment)(benchmark::State& state) {
    for (auto _ : state) {
        const int fd = Connect();
        if (fd < 0) {
            state.SkipWithError("connect failed");
            return;
        }
        ::close(fd);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(TcpServerFixture, ConnectionEstablishment)->Iterations(1000);

} // namespace solar_net

BENCHMARK_MAIN();
