#include "solar_net/net/transport/socket.h"

#include <gtest/gtest.h>

#include "solar_net/net/transport/inet_address.h"

namespace solar_net {
namespace {

TEST(SocketTest, CreateTcpReturnsValidSocket) {
    Socket socket = Socket::CreateTcp();
    EXPECT_TRUE(socket.IsValid());
    EXPECT_GE(socket.Fd(), 0);
}

TEST(SocketTest, CreateUdpReturnsValidSocket) {
    Socket socket = Socket::CreateUdp();
    EXPECT_TRUE(socket.IsValid());
    EXPECT_GE(socket.Fd(), 0);
}

TEST(SocketTest, SetOptionsSucceeds) {
    Socket socket = Socket::CreateTcp();
    ASSERT_TRUE(socket.IsValid());

    EXPECT_TRUE(socket.SetReuseAddr(true));
    EXPECT_TRUE(socket.SetReusePort(true));
    EXPECT_TRUE(socket.SetKeepAlive(true));
    EXPECT_TRUE(socket.SetTcpNoDelay(true));
    EXPECT_TRUE(socket.SetNonBlocking(true));
    EXPECT_TRUE(socket.SetCloseOnExec(true));

    EXPECT_TRUE(socket.SetNonBlocking(false));
    EXPECT_TRUE(socket.SetCloseOnExec(false));
}

TEST(SocketTest, BindAndListen) {
    Socket socket = Socket::CreateTcp();
    ASSERT_TRUE(socket.IsValid());

    EXPECT_TRUE(socket.SetReuseAddr(true));
    EXPECT_TRUE(socket.Bind(InetAddress(0)));
    EXPECT_TRUE(socket.Listen());
}

TEST(SocketTest, AcceptOnEmptyQueueReturnsInvalidFd) {
    Socket socket = Socket::CreateTcp();
    ASSERT_TRUE(socket.IsValid());

    ASSERT_TRUE(socket.SetNonBlocking(true));
    ASSERT_TRUE(socket.Bind(InetAddress(0)));
    ASSERT_TRUE(socket.Listen());

    auto [conn_fd, peer] = socket.Accept();
    EXPECT_EQ(conn_fd, -1);
    EXPECT_EQ(peer.ToIpPort(), "0.0.0.0:0");
}

TEST(SocketTest, MoveTransfersOwnership) {
    Socket first = Socket::CreateTcp();
    ASSERT_TRUE(first.IsValid());
    const int fd = first.Fd();

    Socket second = std::move(first);
    EXPECT_FALSE(first.IsValid());
    EXPECT_TRUE(second.IsValid());
    EXPECT_EQ(second.Fd(), fd);
}

TEST(SocketTest, MoveAssignmentClosesOldFd) {
    Socket first = Socket::CreateTcp();
    Socket second = Socket::CreateTcp();
    ASSERT_TRUE(first.IsValid());
    ASSERT_TRUE(second.IsValid());

    const int first_fd = first.Fd();
    const int second_fd = second.Fd();
    EXPECT_NE(first_fd, second_fd);

    second = std::move(first);
    EXPECT_TRUE(second.IsValid());
    EXPECT_EQ(second.Fd(), first_fd);
    EXPECT_FALSE(first.IsValid());
}

TEST(SocketTest, InvalidSocketOperationsFail) {
    Socket socket;
    EXPECT_FALSE(socket.IsValid());

    EXPECT_FALSE(socket.SetReuseAddr(true));
    EXPECT_FALSE(socket.SetReusePort(true));
    EXPECT_FALSE(socket.SetKeepAlive(true));
    EXPECT_FALSE(socket.SetTcpNoDelay(true));
    EXPECT_FALSE(socket.SetNonBlocking(true));
    EXPECT_FALSE(socket.SetCloseOnExec(true));
    EXPECT_FALSE(socket.Bind(InetAddress(0)));
    EXPECT_FALSE(socket.Listen());
    EXPECT_FALSE(socket.Close());
    EXPECT_FALSE(socket.ShutdownWrite());

    auto [conn_fd, peer] = socket.Accept();
    EXPECT_EQ(conn_fd, -1);
}

TEST(SocketTest, Ipv6SocketCreationAndBinding) {
    Socket socket = Socket::CreateTcp(AF_INET6);
    ASSERT_TRUE(socket.IsValid());

    EXPECT_TRUE(socket.SetReuseAddr(true));
    EXPECT_TRUE(socket.Bind(InetAddress(0, false, AF_INET6)));
    EXPECT_TRUE(socket.Listen());
}

} // namespace
} // namespace solar_net
