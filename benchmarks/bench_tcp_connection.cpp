#include "solar_net/net/transport/tcp_connection.h"

#include "solar_net/base/buffer.h"
#include "solar_net/net/event_loop.h"
#include "solar_net/net/event_loop_thread.h"
#include "solar_net/net/transport/inet_address.h"

#include <benchmark/benchmark.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <thread>

#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

namespace solar_net {

namespace {

class TcpConnectionFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State& state) override {
        int fds[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds) != 0) {
            state.SkipWithError("socketpair failed");
            return;
        }
        m_client_fd = fds[1];

        m_thread = std::make_unique<EventLoopThread>("bench-tcp");
        m_loop = m_thread->Start();
        if (m_loop == nullptr) {
            state.SkipWithError("EventLoopThread start failed");
            return;
        }

        m_conn = std::make_shared<TcpConnection>(m_loop, "bench", fds[0], InetAddress{}, InetAddress{});
        m_conn->SetMessageCallback([](const TcpConnection::TcpConnectionPtr& c, Buffer* buf, Time) {
            c->Send(buf);
        });

        std::promise<void> ready;
        m_loop->RunInLoop([&] {
            m_conn->ConnectEstablished();
            ready.set_value();
        });
        ready.get_future().wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void TearDown(benchmark::State& state) override {
        (void)state;
        if (m_loop != nullptr && m_conn != nullptr) {
            std::promise<void> done;
            m_loop->RunInLoop([&] {
                m_conn->ForceClose();
                m_conn->ConnectDestroyed();
                done.set_value();
            });
            done.get_future().wait();
        }
        if (m_thread != nullptr) {
            m_thread->Stop();
        }
        if (m_client_fd >= 0) {
            ::close(m_client_fd);
            m_client_fd = -1;
        }
    }

    int m_client_fd{-1};
    EventLoop* m_loop{nullptr};
    std::unique_ptr<EventLoopThread> m_thread;
    std::shared_ptr<TcpConnection> m_conn;
};

} // namespace

BENCHMARK_DEFINE_F(TcpConnectionFixture, SendSmall)(benchmark::State& state) {
    const std::string payload(static_cast<size_t>(state.range(0)), 'x');
    for (auto _ : state) {
        m_conn->Send(payload);
    }
    state.SetBytesProcessed(static_cast<int64_t>(payload.size()) * state.iterations());
}
BENCHMARK_REGISTER_F(TcpConnectionFixture, SendSmall)->Arg(64)->Arg(256)->Arg(1024);

BENCHMARK_DEFINE_F(TcpConnectionFixture, EchoRoundTrip)(benchmark::State& state) {
    const std::string payload(static_cast<size_t>(state.range(0)), 'x');
    char buf[8192]{};

    for (auto _ : state) {
        if (::write(m_client_fd, payload.data(), payload.size()) !=
            static_cast<ssize_t>(payload.size())) {
            state.SkipWithError("write failed");
            return;
        }

        size_t total = 0;
        while (total < payload.size()) {
            const ssize_t n = ::read(m_client_fd, buf + total, sizeof(buf) - total);
            if (n > 0) {
                total += static_cast<size_t>(n);
            } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                break;
            } else {
                std::this_thread::yield();
            }
        }
        state.SetBytesProcessed(static_cast<int64_t>(payload.size()));
    }
}
BENCHMARK_REGISTER_F(TcpConnectionFixture, EchoRoundTrip)->Arg(64)->Arg(256)->Arg(1024);

} // namespace solar_net

BENCHMARK_MAIN();
