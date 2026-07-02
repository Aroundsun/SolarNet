#include "solar_net/net/transport/acceptor.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "solar_net/net/event_loop.h"
#include "solar_net/net/event_loop_thread.h"
#include "solar_net/net/transport/inet_address.h"

namespace solar_net {
namespace {

TEST(AcceptorTest, ConstructAndNotListeningByDefault) {
    EventLoop loop;
    Acceptor acceptor(&loop, InetAddress(0));

    EXPECT_FALSE(acceptor.IsListening());
    EXPECT_EQ(acceptor.ListenAddress().ToIpPort(), "0.0.0.0:0");
}

TEST(AcceptorTest, ListenSucceeds) {
    EventLoop loop;
    Acceptor acceptor(&loop, InetAddress(0));

    EXPECT_TRUE(acceptor.Listen());
    EXPECT_TRUE(acceptor.IsListening());
}

TEST(AcceptorTest, ListenIsIdempotent) {
    EventLoop loop;
    Acceptor acceptor(&loop, InetAddress(0));

    EXPECT_TRUE(acceptor.Listen());
    EXPECT_TRUE(acceptor.Listen());
    EXPECT_TRUE(acceptor.IsListening());
}

TEST(AcceptorTest, AcceptsNewConnection) {
    EventLoopThread loop_thread("acceptor-test");
    EventLoop* loop = loop_thread.Start();
    ASSERT_NE(loop, nullptr);

    constexpr uint16_t kTestPort = 29999;

    std::promise<std::pair<int, std::string>> connection_promise;
    auto connection_future = connection_promise.get_future();
    std::shared_ptr<Acceptor> acceptor;

    loop->RunInLoop([&] {
        acceptor = std::make_shared<Acceptor>(loop, InetAddress(kTestPort));
        acceptor->SetNewConnectionCallback([&connection_promise](int sockfd, const InetAddress& peer) {
            connection_promise.set_value({sockfd, peer.ToIpPort()});
        });
        ASSERT_TRUE(acceptor->Listen());
    });

    std::thread client([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kTestPort);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        ::close(fd);
    });

    auto status = connection_future.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(status, std::future_status::ready);

    if (status == std::future_status::ready) {
        auto [fd, peer] = connection_future.get();
        EXPECT_GE(fd, 0);
        EXPECT_FALSE(peer.empty());
        ::close(fd);
    }

    client.join();

    std::promise<void> cleanup_done;
    loop->RunInLoop([&] {
        acceptor.reset();
        cleanup_done.set_value();
    });
    cleanup_done.get_future().wait();

    loop_thread.Stop();
}

TEST(AcceptorTest, NoCallbackClosesFd) {
    EventLoopThread loop_thread("acceptor-test");
    EventLoop* loop = loop_thread.Start();
    ASSERT_NE(loop, nullptr);

    constexpr uint16_t kTestPort = 29998;
    std::shared_ptr<Acceptor> acceptor;

    loop->RunInLoop([&] {
        acceptor = std::make_shared<Acceptor>(loop, InetAddress(kTestPort));
        ASSERT_TRUE(acceptor->Listen());
    });

    std::thread client([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kTestPort);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        ::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        ::close(fd);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    client.join();

    std::promise<void> cleanup_done;
    loop->RunInLoop([&] {
        acceptor.reset();
        cleanup_done.set_value();
    });
    cleanup_done.get_future().wait();

    loop_thread.Stop();
}

TEST(AcceptorTest, Ipv6Listen) {
    EventLoop loop;
    Acceptor acceptor(&loop, InetAddress(0, false, AF_INET6));
    EXPECT_TRUE(acceptor.Listen());
    EXPECT_TRUE(acceptor.IsListening());
}

} // namespace
} // namespace solar_net
