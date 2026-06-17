#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <string>
#include <thread>

#include <unistd.h>

#include "event_loop.h"
#include "event_loop_thread_pool.h"
#include "tcp_connection.h"
#include "tcp_server.h"
#include "tcp_test_util.h"

using solar_net::EventLoop;
using solar_net::TcpConnection;
using solar_net::TcpConnectionPtr;
using solar_net::TcpServer;
using solar_net::test::connect_to;
using solar_net::test::make_loopback_addr;

using namespace std::chrono_literals;

namespace {

void stop_loop_after(EventLoop& loop, std::chrono::milliseconds delay) {
    std::thread([&]() {
        std::this_thread::sleep_for(delay);
        loop.stop();
    }).detach();
}

void run_tcp_server(const std::function<void(EventLoop&, TcpServer&)>& setup) {
    std::promise<void> done;
    auto finished = done.get_future();

    std::thread server_thread([&]() {
        EventLoop loop;
        TcpServer server(&loop, make_loopback_addr(0), "test-server");
        setup(loop, server);
        done.set_value();
    });

    finished.wait();
    server_thread.join();
}

} // namespace

TEST(TcpServerTest, StartExposesListeningPort) {
    run_tcp_server([](EventLoop& loop, TcpServer& server) {
        server.set_thread_num(0);
        server.start();
        EXPECT_GT(server.port(), 0u);
        EXPECT_TRUE(server.thread_pool()->started());
    });
}

TEST(TcpServerTest, AcceptsConnectionSingleThread) {
    std::atomic<bool> connected{false};

    run_tcp_server([&](EventLoop& loop, TcpServer& server) {
        server.set_thread_num(0);
        server.set_connection_callback([&](const TcpConnectionPtr& conn) {
            connected = true;
            EXPECT_EQ(conn->state(), TcpConnection::State::kConnected);
            EXPECT_NE(conn->name().find("test-server-"), std::string::npos);
            loop.stop();
        });

        server.start();
        const uint16_t port = server.port();

        std::thread client([port]() {
            const int fd = connect_to(port);
            if (fd >= 0) {
                ::close(fd);
            }
        });
        client.detach();

        loop.loop();
    });

    EXPECT_TRUE(connected);
}

TEST(TcpServerTest, ReceivesMessageFromClient) {
    std::atomic<bool> received{false};

    run_tcp_server([&](EventLoop& loop, TcpServer& server) {
        server.set_thread_num(0);
        server.set_message_callback([&](const TcpConnectionPtr&, solar_net::Buffer* buf, int64_t) {
            EXPECT_EQ(buf->retrieve_all_as_string(), "ping");
            received = true;
            loop.stop();
        });

        server.start();
        const uint16_t port = server.port();

        std::thread client([port]() {
            const int fd = connect_to(port);
            if (fd >= 0) {
                ::write(fd, "ping", 4);
                ::close(fd);
            }
        });
        client.detach();

        stop_loop_after(loop, 500ms);
        loop.loop();
    });

    EXPECT_TRUE(received);
}

TEST(TcpServerTest, DispatchesConnectionToWorkerLoop) {
    std::atomic<bool> checked{false};

    run_tcp_server([&](EventLoop& loop, TcpServer& server) {
        server.set_thread_num(1);
        server.set_connection_callback([&](const TcpConnectionPtr& conn) {
            EXPECT_NE(conn->get_loop(), &loop);
            checked = true;
            loop.stop();
        });

        server.start();
        const uint16_t port = server.port();

        std::thread client([port]() {
            const int fd = connect_to(port);
            if (fd >= 0) {
                ::close(fd);
            }
        });
        client.detach();

        stop_loop_after(loop, 500ms);
        loop.loop();
    });

    EXPECT_TRUE(checked);
}

TEST(TcpServerTest, ServerCanSendReply) {
    std::promise<std::string> reply;
    auto reply_future = reply.get_future();

    run_tcp_server([&](EventLoop& loop, TcpServer& server) {
        server.set_thread_num(0);
        server.set_message_callback([&](const TcpConnectionPtr& conn, solar_net::Buffer* buf, int64_t) {
            const std::string msg = buf->retrieve_all_as_string();
            if (msg == "hello") {
                conn->send("world");
            }
        });

        server.start();
        const uint16_t port = server.port();

        std::thread client([port, &reply]() {
            const int fd = connect_to(port);
            if (fd < 0) {
                return;
            }

            ::write(fd, "hello", 5);

            char buf[16] = {};
            const ssize_t n = ::read(fd, buf, sizeof(buf));
            if (n > 0) {
                reply.set_value(std::string(buf, static_cast<std::size_t>(n)));
            }

            ::close(fd);
        });
        client.detach();

        stop_loop_after(loop, 500ms);
        loop.loop();
    });

    ASSERT_EQ(reply_future.wait_for(2s), std::future_status::ready);
    EXPECT_EQ(reply_future.get(), "world");
}

TEST(TcpServerTest, ClientCloseDoesNotHang) {
    std::atomic<int> connection_events{0};

    run_tcp_server([&](EventLoop& loop, TcpServer& server) {
        server.set_thread_num(0);
        server.set_connection_callback([&](const TcpConnectionPtr&) {
            if (connection_events.fetch_add(1) + 1 >= 2) {
                loop.stop();
            }
        });

        server.start();
        const uint16_t port = server.port();

        std::thread client([port]() {
            const int fd = connect_to(port);
            if (fd >= 0) {
                ::close(fd);
            }
        });
        client.detach();

        stop_loop_after(loop, 500ms);
        loop.loop();
    });

    EXPECT_GE(connection_events.load(), 1);
}

TEST(TcpServerTest, ThreadInitCallbackInvoked) {
    std::atomic<int> init_count{0};

    run_tcp_server([&](EventLoop& loop, TcpServer& server) {
        server.set_thread_num(1);
        server.set_thread_init_callback([&](EventLoop* io_loop) {
            init_count.fetch_add(1);
            EXPECT_TRUE(io_loop->is_in_loop_thread());
        });

        server.start();
        EXPECT_EQ(init_count.load(), 1);
    });
}
