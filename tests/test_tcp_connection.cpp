#include "solar_net/net/transport/tcp_connection.h"

#include "solar_net/base/buffer.h"
#include "solar_net/net/event_loop.h"
#include "solar_net/net/event_loop_thread.h"
#include "solar_net/net/transport/inet_address.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <string>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

namespace solar_net {
namespace {

class TcpConnectionTest : public ::testing::Test {
protected:
    void TearDown() override {
        if (m_client_fd >= 0) {
            ::close(m_client_fd);
            m_client_fd = -1;
        }
    }

    [[nodiscard]] std::shared_ptr<TcpConnection> MakeConnection(EventLoop* loop, const std::string& name) {
        int fds[2];
        const int ret = ::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, fds);
        EXPECT_EQ(ret, 0);

        m_client_fd = fds[1];
        return std::make_shared<TcpConnection>(loop, name, fds[0], InetAddress{}, InetAddress{});
    }

    void WriteClient(std::string_view data) {
        EXPECT_EQ(::write(m_client_fd, data.data(), data.size()), static_cast<ssize_t>(data.size()));
    }

    [[nodiscard]] std::string ReadClient(size_t max_len) {
        std::string result(max_len, '\0');
        const ssize_t n = ::read(m_client_fd, result.data(), result.size());
        if (n > 0) {
            result.resize(static_cast<size_t>(n));
        } else {
            result.clear();
        }
        return result;
    }

    void CloseConnection(EventLoop* loop, const std::shared_ptr<TcpConnection>& conn) {
        std::promise<void> done;
        loop->RunInLoop([&] {
            conn->ForceClose();
            conn->ConnectDestroyed();
            done.set_value();
        });
        done.get_future().wait();
    }

    int m_client_fd{-1};
};

TEST_F(TcpConnectionTest, InitialState) {
    EventLoop loop;
    const auto conn = MakeConnection(&loop, "initial");

    EXPECT_EQ(conn->Name(), "initial");
    EXPECT_FALSE(conn->IsConnected());
    EXPECT_FALSE(conn->IsDisconnected());
}

TEST_F(TcpConnectionTest, ConnectEstablishedAndDestroyed) {
    EventLoop loop;
    const auto conn = MakeConnection(&loop, "established");

    std::atomic<bool> connected{false};
    std::atomic<bool> disconnected{false};
    conn->SetConnectionCallback([&](const TcpConnection::TcpConnectionPtr& c) {
        if (c->IsConnected()) {
            connected.store(true);
        } else {
            disconnected.store(true);
        }
    });

    conn->ConnectEstablished();
    EXPECT_TRUE(connected.load());
    EXPECT_TRUE(conn->IsConnected());

    conn->ConnectDestroyed();
    EXPECT_TRUE(disconnected.load());
    EXPECT_TRUE(conn->IsDisconnected());
}

TEST_F(TcpConnectionTest, SendFromLoopThread) {
    EventLoopThread thread("tcp-send");
    EventLoop* loop = thread.Start();
    ASSERT_NE(loop, nullptr);

    const auto conn = MakeConnection(loop, "send");
    std::atomic<bool> connected{false};
    conn->SetConnectionCallback([&](const TcpConnection::TcpConnectionPtr& c) {
        if (c->IsConnected()) {
            connected.store(true);
        }
    });

    loop->RunInLoop([&] { conn->ConnectEstablished(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_TRUE(connected.load());

    conn->Send("hello");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(ReadClient(128), "hello");

    CloseConnection(loop, conn);
    thread.Stop();
}

TEST_F(TcpConnectionTest, EchoRoundTrip) {
    EventLoopThread thread("tcp-echo");
    EventLoop* loop = thread.Start();
    ASSERT_NE(loop, nullptr);

    const auto conn = MakeConnection(loop, "echo");
    std::atomic<int> message_count{0};
    conn->SetMessageCallback([&](const TcpConnection::TcpConnectionPtr& c, Buffer* buf, Time) {
        c->Send(buf);
        message_count.fetch_add(1, std::memory_order_relaxed);
    });

    loop->RunInLoop([&] { conn->ConnectEstablished(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    WriteClient("hello");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(message_count.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(ReadClient(128), "hello");

    CloseConnection(loop, conn);
    thread.Stop();
}

TEST_F(TcpConnectionTest, PeerClose) {
    EventLoopThread thread("tcp-peer-close");
    EventLoop* loop = thread.Start();
    ASSERT_NE(loop, nullptr);

    const auto conn = MakeConnection(loop, "peer_close");
    std::atomic<bool> closed{false};
    conn->SetCloseCallback([&](const TcpConnection::TcpConnectionPtr&) {
        closed.store(true);
    });

    loop->RunInLoop([&] { conn->ConnectEstablished(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ::close(m_client_fd);
    m_client_fd = -1;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(closed.load());
    EXPECT_TRUE(conn->IsDisconnected());

    loop->RunInLoop([&] { conn->ConnectDestroyed(); });
    thread.Stop();
}

TEST_F(TcpConnectionTest, HighWaterMark) {
    EventLoopThread thread("tcp-hwm");
    EventLoop* loop = thread.Start();
    ASSERT_NE(loop, nullptr);

    const auto conn = MakeConnection(loop, "hwm");
    conn->SetHighWaterMarkCallback([](const TcpConnection::TcpConnectionPtr&, size_t) {}, 64);

    loop->RunInLoop([&] { conn->ConnectEstablished(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const std::string large(128, 'x');
    conn->Send(large);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(ReadClient(256).size(), 128u);

    CloseConnection(loop, conn);
    thread.Stop();
}

TEST_F(TcpConnectionTest, CrossThreadSend) {
    EventLoopThread thread("tcp-cross");
    EventLoop* loop = thread.Start();
    ASSERT_NE(loop, nullptr);

    const auto conn = MakeConnection(loop, "cross");
    loop->RunInLoop([&] { conn->ConnectEstablished(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    std::thread sender([&] { conn->Send("from another thread"); });
    sender.join();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(ReadClient(128), "from another thread");

    CloseConnection(loop, conn);
    thread.Stop();
}

TEST_F(TcpConnectionTest, Context) {
    EventLoop loop;
    const auto conn = MakeConnection(&loop, "context");

    conn->SetContext(std::string{"payload"});
    EXPECT_EQ(std::any_cast<std::string>(conn->GetContext()), "payload");
}

} // namespace
} // namespace solar_net
