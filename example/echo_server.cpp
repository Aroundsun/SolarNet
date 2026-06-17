#include <arpa/inet.h>
#include <csignal>
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>

#include "event_loop.h"
#include "log.h"
#include "tcp_connection.h"
#include "tcp_server.h"

namespace {

solar_net::EventLoop* g_loop = nullptr;

void on_signal(int) {
    if (g_loop) {
        g_loop->stop();
    }
}

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [-p port] [-t threads] [-v]\n"
              << "  -p port     listen port, default 8080\n"
              << "  -t threads  IO thread count, default 0\n"
              << "  -v          enable library debug logs (spdlog)\n";
}

} // namespace

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    int threads = 0;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if ((arg == "-t" || arg == "--threads") && i + 1 < argc) {
            threads = std::atoi(argv[++i]);
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else {
            std::cerr << "unknown argument: " << arg << '\n';
            print_usage(argv[0]);
            return 1;
        }
    }

    solar_net::log::Options log_opts;
    log_opts.level = verbose ? solar_net::log::Level::Debug : solar_net::log::Level::Info;
    if (!solar_net::log::init(log_opts)) {
        std::cerr << "failed to initialize logging\n";
        return 1;
    }

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    solar_net::EventLoop loop;
    g_loop = &loop;

    ::sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    solar_net::TcpServer server(&loop, addr, "echo-server");
    server.set_thread_num(threads);

    server.set_connection_callback([](const solar_net::TcpConnectionPtr& conn) {
        if (conn->state() == solar_net::TcpConnection::State::kConnected) {
            SNLOG_INFO("connected: {}", conn->name());
        } else {
            SNLOG_INFO("disconnected: {}", conn->name());
        }
    });

    server.set_message_callback([](const solar_net::TcpConnectionPtr& conn,
                                   solar_net::Buffer* buf,
                                   int64_t) {
        conn->send(buf);
    });

    server.start();

    SNLOG_INFO("listening on port {}, IO threads={}", server.port(), threads);
    SNLOG_INFO("test: echo hello | nc 127.0.0.1 {}", server.port());

    loop.loop();
    server.stop();

    g_loop = nullptr;
    SNLOG_INFO("stopped");
    solar_net::log::shutdown();
    return 0;
}
