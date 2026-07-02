#include "solar_net/net/transport/socket.h"
#include "solar_net/net/transport/inet_address.h"

#include <format>
#include <iostream>

#include <unistd.h>

int main() {
    solar_net::Socket listen_socket = solar_net::Socket::CreateTcp();
    if (!listen_socket.IsValid()) {
        std::cerr << "failed to create socket\n";
        return 1;
    }

    listen_socket.SetReuseAddr(true);
    listen_socket.SetReusePort(true);
    listen_socket.SetTcpNoDelay(true);
    listen_socket.SetKeepAlive(true);

    if (!listen_socket.Bind(solar_net::InetAddress(8080))) {
        std::cerr << "failed to bind\n";
        return 1;
    }

    if (!listen_socket.Listen()) {
        std::cerr << "failed to listen\n";
        return 1;
    }

    std::cout << std::format("listening fd={} on 0.0.0.0:8080\n", listen_socket.Fd());

    auto [conn_fd, peer] = listen_socket.Accept();
    if (conn_fd >= 0) {
        std::cout << std::format("accepted fd={} from {}\n", conn_fd, peer.ToIpPort());
        ::close(conn_fd);
    } else {
        std::cout << "no pending connection (expected in this example)\n";
    }

    return 0;
}
