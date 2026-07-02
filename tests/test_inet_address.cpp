#include "solar_net/net/transport/inet_address.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <string>

namespace solar_net {
namespace {

TEST(InetAddressTest, DefaultConstructsToAnyZero) {
    const InetAddress addr;
    EXPECT_EQ(addr.Family(), AF_INET);
    EXPECT_EQ(addr.ToIp(), "0.0.0.0");
    EXPECT_EQ(addr.Port(), 0);
    EXPECT_EQ(addr.ToIpPort(), "0.0.0.0:0");
}

TEST(InetAddressTest, ConstructFromPort) {
    const InetAddress addr(8080);
    EXPECT_EQ(addr.ToIp(), "0.0.0.0");
    EXPECT_EQ(addr.Port(), 8080);
    EXPECT_EQ(addr.ToIpPort(), "0.0.0.0:8080");
}

TEST(InetAddressTest, ConstructLoopbackPort) {
    const InetAddress addr(8080, true);
    EXPECT_EQ(addr.ToIp(), "127.0.0.1");
    EXPECT_EQ(addr.Port(), 8080);
    EXPECT_EQ(addr.ToIpPort(), "127.0.0.1:8080");
}

TEST(InetAddressTest, ConstructFromIpAndPort) {
    const InetAddress addr("192.168.1.1", 8080);
    EXPECT_EQ(addr.ToIp(), "192.168.1.1");
    EXPECT_EQ(addr.Port(), 8080);
    EXPECT_EQ(addr.ToIpPort(), "192.168.1.1:8080");
}

TEST(InetAddressTest, ConstructFromSockaddrIn) {
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(9090);
    inet_pton(AF_INET, "10.0.0.1", &sin.sin_addr);

    const InetAddress addr(sin);
    EXPECT_EQ(addr.ToIp(), "10.0.0.1");
    EXPECT_EQ(addr.Port(), 9090);
    EXPECT_EQ(addr.GetSockLen(), sizeof(sockaddr_in));
}

TEST(InetAddressTest, ConstructFromSockaddrStorage) {
    sockaddr_storage ss{};
    auto* sin = reinterpret_cast<sockaddr_in*>(&ss);
    sin->sin_family = AF_INET;
    sin->sin_port = htons(7070);
    inet_pton(AF_INET, "172.16.0.1", &sin->sin_addr);

    const InetAddress addr(ss);
    EXPECT_EQ(addr.ToIp(), "172.16.0.1");
    EXPECT_EQ(addr.Port(), 7070);
}

TEST(InetAddressTest, ConstructFromSockaddrPointer) {
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(6060);
    inet_pton(AF_INET, "1.2.3.4", &sin.sin_addr);

    const InetAddress addr(reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
    EXPECT_EQ(addr.ToIp(), "1.2.3.4");
    EXPECT_EQ(addr.Port(), 6060);
}

TEST(InetAddressTest, EqualityWorks) {
    const InetAddress a1("192.168.1.1", 8080);
    const InetAddress a2("192.168.1.1", 8080);
    const InetAddress b("192.168.1.2", 8080);

    EXPECT_EQ(a1, a2);
    EXPECT_NE(a1, b);
}

TEST(InetAddressTest, LessThanWorksForMap) {
    const InetAddress a("192.168.1.1", 8080);
    const InetAddress b("192.168.1.2", 8080);

    std::map<InetAddress, int> m;
    m[a] = 1;
    m[b] = 2;

    EXPECT_EQ(m.size(), 2);
    EXPECT_EQ(m.at(a), 1);
    EXPECT_EQ(m.at(b), 2);
}

TEST(InetAddressTest, CopyWorks) {
    const InetAddress original("10.0.0.1", 1234);
    const InetAddress copy = original;

    EXPECT_EQ(original, copy);
    EXPECT_EQ(copy.ToIpPort(), "10.0.0.1:1234");
}

TEST(InetAddressTest, InvalidIpFallsBackToAny) {
    const InetAddress addr("not.an.ip.address", 8080);
    EXPECT_EQ(addr.ToIp(), "0.0.0.0");
    EXPECT_EQ(addr.Port(), 0);
}

TEST(InetAddressTest, Ipv6Loopback) {
    const InetAddress addr(8080, true, AF_INET6);
    EXPECT_EQ(addr.Family(), AF_INET6);
    EXPECT_EQ(addr.ToIp(), "::1");
    EXPECT_EQ(addr.Port(), 8080);
    EXPECT_EQ(addr.ToIpPort(), "[::1]:8080");
}

TEST(InetAddressTest, Ipv6FromIpAndPort) {
    const InetAddress addr("2001:db8::1", 8080, AF_INET6);
    EXPECT_EQ(addr.Family(), AF_INET6);
    EXPECT_EQ(addr.ToIp(), "2001:db8::1");
    EXPECT_EQ(addr.Port(), 8080);
    EXPECT_EQ(addr.ToIpPort(), "[2001:db8::1]:8080");
}

TEST(InetAddressTest, NullSockaddrFallsBackToAny) {
    const InetAddress addr(nullptr, sizeof(sockaddr_in));
    EXPECT_EQ(addr.ToIp(), "0.0.0.0");
    EXPECT_EQ(addr.Port(), 0);
}

} // namespace
} // namespace solar_net
