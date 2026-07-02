#include "solar_net/net/transport/tcp_server.h"

#include "solar_net/base/buffer.h"
#include "solar_net/net/event_loop_thread.h"
#include "solar_net/net/transport/inet_address.h"
#include "solar_net/net/transport/tcp_connection.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

namespace solar_net {
namespace {

void WaitUntilListening(TcpServer* server, std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (server->ListenAddress().Port() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void WaitUntilStopped(TcpServer* server, std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!server->IsFullyStopped() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

class TcpServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_loop_thread = std::make_unique<EventLoopThread>("tcp-server-test");
        m_loop = m_loop_thread->Start();
        ASSERT_NE(m_loop, nullptr);
        m_server = std::make_unique<TcpServer>(m_loop, InetAddress(0), "test-server");
    }

    void TearDown() override {
        if (m_server) {
            m_server->Stop();
            WaitUntilStopped(m_server.get());
            m_server.reset();
        }
        if (m_loop_thread) {
            m_loop_thread->Stop();
        }
    }

    [[nodiscard]] int ConnectClient(const InetAddress& addr) const {
        const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            return -1;
        }
        if (::connect(fd, addr.GetSockAddr(), addr.GetSockLen()) != 0) {
            ::close(fd);
            return -1;
        }
        return fd;
    }

    EventLoop* m_loop{nullptr};
    std::unique_ptr<EventLoopThread> m_loop_thread;
    std::unique_ptr<TcpServer> m_server;
};

TEST_F(TcpServerTest, StartStop) {
    m_server->SetThreadNum(2);
    m_server->Start();
    WaitUntilListening(m_server.get());

    EXPECT_NE(m_server->ListenAddress().Port(), 0);
    m_server->Stop();
}

TEST_F(TcpServerTest, SingleConnectionEcho) {
    m_server->SetThreadNum(0);

    std::atomic<int> connection_count{0};
    std::atomic<int> message_count{0};

    m_server->SetConnectionCallback([&](const TcpConnection::TcpConnectionPtr& conn) {
        if (conn->IsConnected()) {
            connection_count.fetch_add(1, std::memory_order_relaxed);
        }
    });
    m_server->SetMessageCallback([&](const TcpConnection::TcpConnectionPtr& conn, Buffer* buf, Time) {
        message_count.fetch_add(1, std::memory_order_relaxed);
        conn->Send(buf);
    });

    m_server->Start();
    WaitUntilListening(m_server.get());

    const InetAddress listen_addr = m_server->ListenAddress();
    const int client_fd = ConnectClient(listen_addr);
    ASSERT_GE(client_fd, 0);

    constexpr const char* kRequest = "hello";
    ASSERT_EQ(::write(client_fd, kRequest, std::strlen(kRequest)),
              static_cast<ssize_t>(std::strlen(kRequest)));

    char response[128]{};
    int total = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (total < static_cast<int>(std::strlen(kRequest)) &&
           std::chrono::steady_clock::now() < deadline) {
        const ssize_t n = ::read(client_fd, response + total,
                                 static_cast<size_t>(sizeof(response) - static_cast<size_t>(total)));
        if (n > 0) {
            total += static_cast<int>(n);
        } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            break;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    EXPECT_EQ(total, static_cast<int>(std::strlen(kRequest)));
    EXPECT_EQ(std::string_view(response, static_cast<size_t>(total)), "hello");
    EXPECT_EQ(connection_count.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(message_count.load(std::memory_order_relaxed), 1);

    ::close(client_fd);
}

TEST_F(TcpServerTest, MultiThreadDistribution) {
    m_server->SetThreadNum(3);

    std::atomic<int> connection_count{0};
    m_server->SetConnectionCallback([&](const TcpConnection::TcpConnectionPtr& conn) {
        if (conn->IsConnected()) {
            connection_count.fetch_add(1, std::memory_order_relaxed);
        }
    });

    m_server->Start();
    WaitUntilListening(m_server.get());

    const InetAddress listen_addr = m_server->ListenAddress();
    std::vector<int> clients;
    for (int i = 0; i < 6; ++i) {
        const int fd = ConnectClient(listen_addr);
        ASSERT_GE(fd, 0) << "failed to connect client " << i;
        clients.push_back(fd);
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (connection_count.load(std::memory_order_relaxed) < 6 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(connection_count.load(std::memory_order_relaxed), 6);

    for (int fd : clients) {
        ::close(fd);
    }
}

} // namespace
} // namespace solar_net
