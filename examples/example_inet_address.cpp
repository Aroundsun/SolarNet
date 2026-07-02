#include "solar_net/net/transport/inet_address.h"

#include <format>
#include <iostream>

int main() {
    const solar_net::InetAddress any(8080);
    std::cout << std::format("any:      {}\n", any.ToIpPort());

    const solar_net::InetAddress loopback(8080, true);
    std::cout << std::format("loopback: {}\n", loopback.ToIpPort());

    const solar_net::InetAddress ipv4("192.168.1.1", 8080);
    std::cout << std::format("ipv4:     {}\n", ipv4.ToIpPort());

    const solar_net::InetAddress ipv6(8080, true, AF_INET6);
    std::cout << std::format("ipv6:     {}\n", ipv6.ToIpPort());

    return 0;
}
