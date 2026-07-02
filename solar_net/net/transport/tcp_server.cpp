#include "solar_net/net/transport/tcp_server.h"

#include "solar_net/net/event_loop.h"
#include "solar_net/net/event_loop_thread_pool.h"
#include "solar_net/base/logger.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <format>
#include <vector>

#include <sys/socket.h>
#include <unistd.h>

namespace solar_net {

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listen_addr, std::string name)
    : m_loop(loop)
    , m_name(std::move(name))
    , m_listen_addr(listen_addr)
    , m_acceptor(std::make_unique<Acceptor>(loop, listen_addr)) {
    assert(loop != nullptr);

    m_acceptor->SetNewConnectionCallback([this](int sockfd, const InetAddress& peer_addr) {
        NewConnection(sockfd, peer_addr);
    });
}

TcpServer::~TcpServer() {
    Stop();
}

void TcpServer::SetThreadNum(size_t thread_num) {
    if (m_started.load(std::memory_order_acquire)) {
        LOG_WARN("TcpServer::SetThreadNum called after Start()");
        return;
    }
    m_thread_num = thread_num;
}

void TcpServer::SetHighWaterMarkCallback(HighWaterMarkCallback cb, size_t high_water_mark) {
    m_high_water_mark = high_water_mark;
    m_high_water_mark_callback = std::move(cb);
}

void TcpServer::Start() {
    if (!m_started.load(std::memory_order_acquire)) {
        m_loop->RunInLoop([this] { StartInLoop(); });
    }
}

void TcpServer::StartInLoop() {
    m_loop->AssertInLoopThread();
    if (m_started.load(std::memory_order_acquire)) {
        return;
    }

    if (m_thread_num > 0) {
        const std::string pool_name = m_name.empty() ? "tcp-server" : m_name;
        m_thread_pool = std::make_unique<EventLoopThreadPool>(m_thread_num, pool_name + "-worker");
        m_thread_pool->Start();
    }

    if (!m_acceptor->Listen()) {
        LOG_ERROR(std::format("TcpServer::StartInLoop [{}] failed to listen on {}",
                              m_name, m_listen_addr.ToIpPort()));
        return;
    }

    m_listen_addr = m_acceptor->ListenAddress();
    m_started.store(true, std::memory_order_release);
    m_stop_complete.store(false, std::memory_order_release);
    LOG_INFO(std::format("TcpServer::StartInLoop [{}] listening on {}", m_name, m_listen_addr.ToIpPort()));
}

void TcpServer::Stop() {
    if (m_started.load(std::memory_order_acquire)) {
        m_loop->RunInLoop([this] { StopInLoop(); });
    }
}

void TcpServer::StopInLoop() {
    m_loop->AssertInLoopThread();
    if (!m_started.load(std::memory_order_acquire)) {
        return;
    }
    m_started.store(false, std::memory_order_release);

    m_acceptor.reset();

    if (m_connections.empty()) {
        StopThreadPoolInLoop();
        LOG_INFO(std::format("TcpServer::StopInLoop [{}] stopped", m_name));
        return;
    }

    m_stop_complete.store(false, std::memory_order_release);
    std::vector<TcpConnection::TcpConnectionPtr> connections;
    connections.reserve(m_connections.size());
    for (const auto& [name, conn] : m_connections) {
        (void)name;
        connections.push_back(conn);
    }
    for (const auto& conn : connections) {
        conn->ForceClose();
    }

    LOG_INFO(std::format("TcpServer::StopInLoop [{}] closing {} connection(s)",
                         m_name, connections.size()));
}

void TcpServer::StopThreadPoolInLoop() {
    m_loop->AssertInLoopThread();
    if (m_thread_pool) {
        m_thread_pool->Stop();
        m_thread_pool.reset();
    }
    m_stop_complete.store(true, std::memory_order_release);
}

void TcpServer::NewConnection(int sockfd, const InetAddress& peer_addr) {
    m_loop->AssertInLoopThread();
    assert(sockfd >= 0);

    EventLoop* io_loop = m_loop;
    if (m_thread_pool != nullptr) {
        io_loop = m_thread_pool->GetNextLoop();
        if (io_loop == nullptr) {
            io_loop = m_loop;
        }
    }

    sockaddr_storage local_storage{};
    socklen_t len = sizeof(local_storage);
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local_storage), &len) != 0) {
        LOG_ERROR(std::format("TcpServer::NewConnection [{}] getsockname failed: {}",
                              m_name, std::strerror(errno)));
        ::close(sockfd);
        return;
    }
    const InetAddress local_addr{reinterpret_cast<const sockaddr*>(&local_storage), len};

    const int conn_id = m_next_conn_id.fetch_add(1, std::memory_order_relaxed);
    const std::string conn_name = std::format("{}-{}#{}@{}",
                                              m_name.empty() ? "conn" : m_name,
                                              peer_addr.ToIpPort(),
                                              conn_id,
                                              io_loop == m_loop ? "main" : "worker");

    auto conn = std::make_shared<TcpConnection>(io_loop, conn_name, sockfd, local_addr, peer_addr);
    conn->SetConnectionCallback(m_connection_callback);
    conn->SetMessageCallback(m_message_callback);
    conn->SetWriteCompleteCallback(m_write_complete_callback);
    if (m_high_water_mark_callback) {
        conn->SetHighWaterMarkCallback(m_high_water_mark_callback, m_high_water_mark);
    }
    conn->SetCloseCallback([this](const TcpConnection::TcpConnectionPtr& c) {
        RemoveConnection(c);
    });

    m_connections[conn_name] = conn;
    io_loop->RunInLoop([conn] { conn->ConnectEstablished(); });
}

void TcpServer::RemoveConnection(const TcpConnection::TcpConnectionPtr& conn) {
    m_loop->RunInLoop([this, conn] { RemoveConnectionInLoop(conn); });
}

void TcpServer::RemoveConnectionInLoop(const TcpConnection::TcpConnectionPtr& conn) {
    m_loop->AssertInLoopThread();
    m_connections.erase(conn->Name());
    conn->GetLoop()->RunInLoop([conn] { conn->ConnectDestroyed(); });

    if (m_connections.empty() && !m_started.load(std::memory_order_acquire)) {
        StopThreadPoolInLoop();
        LOG_INFO(std::format("TcpServer::RemoveConnectionInLoop [{}] all connections closed", m_name));
    }
}

} // namespace solar_net
