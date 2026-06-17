#include "tcp_server.h"
#include "acceptor.h"
#include "event_loop.h"
#include "event_loop_thread_pool.h"
#include "tcp_connection.h"
#include "socket.h"

#include <cstdio>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace solar_net {

TcpServer::TcpServer(EventLoop* loop,
                       const ::sockaddr_in& listen_addr,
                       const std::string& name)
    : loop_(loop)
    , name_(name)
    , acceptor_(std::make_unique<Acceptor>(loop, listen_addr))
    , thread_pool_(std::make_shared<EventLoopThreadPool>(loop))
    , started_(false) {
    acceptor_->set_new_connection_callback(
        [this](int fd, const ::sockaddr_in& peer_addr) {
            new_connection(fd, peer_addr);
        }
    );
}

TcpServer::~TcpServer() = default;

void TcpServer::start() {
    if (!started_) {
        started_ = true;
        thread_pool_->start(thread_init_cb_);
    }

    if (!acceptor_->listening()) {
        loop_->run_in_loop([this]() { acceptor_->listen(); });
    }
}

void TcpServer::set_thread_num(int num_threads) {
    thread_pool_->set_thread_num(num_threads);
}

uint16_t TcpServer::port() const {
    return acceptor_->port();
}

void TcpServer::new_connection(int fd, const ::sockaddr_in& peer_addr) {
    loop_->assert_in_loop_thread();

    EventLoop* io_loop = thread_pool_->get_next_loop();

    char buf[64] = {};
    ::inet_ntop(AF_INET, &peer_addr.sin_addr, buf, sizeof(buf));
    std::string conn_name = name_ + "-" + std::to_string(next_conn_id_++) + "#" +
                             std::string(buf) + ":" + std::to_string(::ntohs(peer_addr.sin_port));

    ::sockaddr_in local_addr = Socket::get_local_addr(fd);

    auto conn = std::make_shared<TcpConnection>(io_loop,
                                                   conn_name,
                                                   fd,
                                                   local_addr,
                                                   peer_addr);

    connections_[conn_name] = conn;

    conn->set_connection_callback(connection_cb_);
    conn->set_message_callback(message_cb_);
    conn->set_write_complete_callback(write_complete_cb_);
    conn->set_close_callback(
        [this](const std::shared_ptr<TcpConnection>& c) {
            remove_connection(c);
        }
    );

    io_loop->run_in_loop([conn]() {
        conn->connection_established();
    });
}

void TcpServer::remove_connection(const std::shared_ptr<TcpConnection>& conn) {
    loop_->run_in_loop([this, conn]() {
        remove_connection_in_loop(conn);
    });
}

void TcpServer::remove_connection_in_loop(const std::shared_ptr<TcpConnection>& conn) {
    loop_->assert_in_loop_thread();

    std::string name = conn->name();
    auto it = connections_.find(name);
    if (it != connections_.end()) {
        connections_.erase(it);
    }

    EventLoop* io_loop = conn->get_loop();
    io_loop->queue_in_loop([conn]() {
        conn->connection_destroyed();
    });
}

} // namespace solar_net
