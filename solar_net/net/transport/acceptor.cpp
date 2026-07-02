#include "solar_net/net/transport/acceptor.h"

#include "solar_net/net/event_loop.h"
#include "solar_net/base/logger.h"

#include <format>
#include <sys/socket.h>
#include <unistd.h>

namespace solar_net {

namespace {

constexpr int kDefaultBacklog = 128;

} // namespace

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuse_port)
    : m_loop(loop)
    , m_listen_addr(listen_addr)
    , m_reuse_port(reuse_port)
    , m_listen_socket(Socket::CreateTcp(listen_addr.Family())) {
    if (m_listen_socket.IsValid()) {
        m_channel = std::make_unique<Channel>(loop, m_listen_socket.Fd());
        m_channel->SetReadCallback([this](Time) { HandleRead(); });
    }
}

Acceptor::~Acceptor() {
    if (m_channel == nullptr) {
        return;
    }

    if (m_loop != nullptr && m_loop->IsInLoopThread()) {
        m_channel->DisableAll();
        m_channel->Remove();
    }

    m_channel.reset();
}

void Acceptor::SetNewConnectionCallback(NewConnectionCallback cb) {
    m_new_connection_callback = std::move(cb);
}

bool Acceptor::Listen() {
    if (m_listening) {
        return true;
    }

    m_loop->AssertInLoopThread();

    if (!m_listen_socket.IsValid()) {
        LOG_ERROR("Acceptor::Listen: invalid listening socket");
        return false;
    }

    if (m_channel == nullptr) {
        LOG_ERROR("Acceptor::Listen: channel not created");
        return false;
    }

    if (!m_listen_socket.SetReuseAddr(true)) {
        return false;
    }

    if (m_reuse_port && !m_listen_socket.SetReusePort(true)) {
        return false;
    }

    if (!m_listen_socket.Bind(m_listen_addr)) {
        return false;
    }

    if (!m_listen_socket.Listen(kDefaultBacklog)) {
        return false;
    }

    if (m_listen_addr.Port() == 0) {
        sockaddr_storage actual_addr{};
        socklen_t len = sizeof(actual_addr);
        if (::getsockname(m_listen_socket.Fd(), reinterpret_cast<sockaddr*>(&actual_addr), &len) == 0) {
            m_listen_addr = InetAddress{reinterpret_cast<const sockaddr*>(&actual_addr), len};
        }
    }

    m_channel->EnableReading();
    m_listening = true;
    return true;
}

void Acceptor::HandleRead() {
    m_loop->AssertInLoopThread();

    while (true) {
        auto [conn_fd, peer_addr] = m_listen_socket.Accept();
        if (conn_fd < 0) {
            break;
        }

        if (m_new_connection_callback) {
            m_new_connection_callback(conn_fd, peer_addr);
        } else {
            ::close(conn_fd);
        }
    }
}

} // namespace solar_net
