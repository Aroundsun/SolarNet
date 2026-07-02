#include "solar_net/net/transport/acceptor.h"
#include "solar_net/net/transport/tcp_connection.h"
#include "solar_net/base/buffer.h"
#include "solar_net/base/logger.h"
#include "solar_net/base/time.h"
#include "solar_net/net/event_loop.h"
#include "solar_net/net/event_loop_thread.h"
#include "solar_net/net/transport/inet_address.h"

#include <csignal>
#include <cerrno>
#include <format>
#include <future>
#include <iostream>
#include <memory>

#include <sys/socket.h>
#include <unistd.h>

int main() {
    std::signal(SIGPIPE, SIG_IGN);

    solar_net::EventLoopThread loop_thread("tcp-echo");
    solar_net::EventLoop* loop = loop_thread.Start();
    if (loop == nullptr) {
        std::cerr << "failed to start event loop thread\n";
        return 1;
    }

    const solar_net::InetAddress listen_addr{12345};
    std::shared_ptr<solar_net::Acceptor> acceptor;

    loop->RunInLoop([&] {
        acceptor = std::make_shared<solar_net::Acceptor>(loop, listen_addr);
        acceptor->SetNewConnectionCallback([loop](int sockfd, const solar_net::InetAddress& peer_addr) {
            sockaddr_storage local_storage{};
            socklen_t len = sizeof(local_storage);
            if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local_storage), &len) != 0) {
                LOG_ERROR(std::format("getsockname failed: {}", std::strerror(errno)));
                ::close(sockfd);
                return;
            }
            const solar_net::InetAddress local_addr{reinterpret_cast<const sockaddr*>(&local_storage), len};

            auto conn = std::make_shared<solar_net::TcpConnection>(
                loop,
                std::format("{}:{}", peer_addr.ToIp(), peer_addr.Port()),
                sockfd,
                local_addr,
                peer_addr);

            conn->SetConnectionCallback([](const solar_net::TcpConnection::TcpConnectionPtr& c) {
                std::cout << std::format("connection {} is {}\n",
                                         c->Name(),
                                         c->IsConnected() ? "up" : "down");
            });

            conn->SetMessageCallback([](const solar_net::TcpConnection::TcpConnectionPtr& c,
                                        solar_net::Buffer* buf,
                                        solar_net::Time) {
                c->Send(buf);
            });

            conn->SetCloseCallback([conn](const solar_net::TcpConnection::TcpConnectionPtr&) {
                std::cout << std::format("connection {} closed\n", conn->Name());
                conn->GetLoop()->RunInLoop([conn] { conn->ConnectDestroyed(); });
            });

            conn->ConnectEstablished();
        });

        if (!acceptor->Listen()) {
            LOG_ERROR("listen failed");
            loop->Quit();
        }
    });

    std::cout << std::format("echo server listening on {}\n", listen_addr.ToIpPort());
    std::cout << "connect with: nc 127.0.0.1 12345\n";
    std::cout << "press enter to quit\n";
    std::cin.get();

    std::promise<void> cleanup_done;
    loop->RunInLoop([&] {
        acceptor.reset();
        cleanup_done.set_value();
    });
    cleanup_done.get_future().wait();

    loop_thread.Stop();
    return 0;
}
