#include "solar_net/net/transport/tcp_server.h"

#include "solar_net/base/buffer.h"
#include "solar_net/base/time.h"
#include "solar_net/net/event_loop.h"
#include "solar_net/net/transport/inet_address.h"
#include "solar_net/net/transport/tcp_connection.h"

#include <csignal>
#include <cstdint>
#include <format>
#include <iostream>
#include <string>
#include <thread>

#include <chrono>

int main(int argc, char* argv[]) {
    std::signal(SIGPIPE, SIG_IGN);

    uint16_t port = 12345;
    size_t thread_num = 0;
    if (argc >= 2) {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }
    if (argc >= 3) {
        thread_num = static_cast<size_t>(std::stoi(argv[2]));
    }

    solar_net::EventLoop loop;
    std::thread loop_thread([&] { loop.Loop(); });

    solar_net::TcpServer server(&loop, solar_net::InetAddress(port), "echo");
    server.SetThreadNum(thread_num);

    server.SetConnectionCallback([](const solar_net::TcpConnection::TcpConnectionPtr& conn) {
        std::cout << std::format("connection {} is {}\n",
                                 conn->Name(),
                                 conn->IsConnected() ? "up" : "down");
    });

    server.SetMessageCallback([](const solar_net::TcpConnection::TcpConnectionPtr& conn,
                               solar_net::Buffer* buf,
                               solar_net::Time) {
        conn->Send(buf);
    });

    server.SetHighWaterMarkCallback([](const solar_net::TcpConnection::TcpConnectionPtr& conn, size_t len) {
        std::cout << std::format("high water mark on {}: {} bytes\n", conn->Name(), len);
    }, 64 * 1024);

    server.Start();

    while (server.ListenAddress().Port() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << std::format("echo server listening on {} with {} IO threads\n",
                             server.ListenAddress().ToIpPort(),
                             thread_num);
    std::cout << "connect with: nc 127.0.0.1 " << server.ListenAddress().Port() << '\n';
    std::cout << "press enter to quit\n";
    std::cin.get();

    server.Stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    loop.Quit();
    loop_thread.join();
    return 0;
}
