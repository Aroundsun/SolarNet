#include "solar_net/net/transport/socket.h"

#include "solar_net/base/logger.h"

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <format>

namespace solar_net {

namespace {

constexpr int kInvalidFd = -1;

bool SetSocketOption(int fd,
                     int level,
                     int optname,
                     const void* optval,
                     socklen_t optlen,
                     std::string_view name) {
    if (fd < 0) {
        LOG_ERROR(std::format("cannot set {} on invalid socket", name));
        return false;
    }
    if (::setsockopt(fd, level, optname, optval, optlen) != 0) {
        LOG_ERROR(std::format("setsockopt {} failed: {}", name, std::strerror(errno)));
        return false;
    }
    return true;
}

} // namespace

Socket::Socket(int fd) noexcept : m_fd(fd) {}

Socket::Socket(Socket&& other) noexcept : m_fd(other.m_fd) {
    other.m_fd = kInvalidFd;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        Close();
        m_fd = other.m_fd;
        other.m_fd = kInvalidFd;
    }
    return *this;
}

Socket::~Socket() {
    Close();
}

Socket Socket::CreateTcp(sa_family_t family) {
    const int fd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd < 0) {
        LOG_ERROR(std::format("socket() failed: {}", std::strerror(errno)));
        return Socket{};
    }
    return Socket{fd};
}

Socket Socket::CreateUdp(sa_family_t family) {
    const int fd = ::socket(family, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
    if (fd < 0) {
        LOG_ERROR(std::format("socket() failed: {}", std::strerror(errno)));
        return Socket{};
    }
    return Socket{fd};
}

bool Socket::Bind(const InetAddress& addr) {
    if (!IsValid()) {
        LOG_ERROR("cannot bind invalid socket");
        return false;
    }
    if (::bind(m_fd, addr.GetSockAddr(), addr.GetSockLen()) != 0) {
        LOG_ERROR(std::format("bind() failed: {}", std::strerror(errno)));
        return false;
    }
    return true;
}

bool Socket::Listen(int backlog) {
    if (!IsValid()) {
        LOG_ERROR("cannot listen on invalid socket");
        return false;
    }
    if (::listen(m_fd, backlog) != 0) {
        LOG_ERROR(std::format("listen() failed: {}", std::strerror(errno)));
        return false;
    }
    return true;
}

std::pair<int, InetAddress> Socket::Accept() {
    if (!IsValid()) {
        LOG_ERROR("cannot accept on invalid socket");
        return {kInvalidFd, InetAddress{}};
    }

    sockaddr_storage peer_addr{};
    socklen_t len = sizeof(peer_addr);
    const int conn_fd =
        ::accept4(m_fd, reinterpret_cast<sockaddr*>(&peer_addr), &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (conn_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            LOG_ERROR(std::format("accept4() failed: {}", std::strerror(errno)));
        }
        return {kInvalidFd, InetAddress{}};
    }

    return {conn_fd, InetAddress{reinterpret_cast<const sockaddr*>(&peer_addr), len}};
}

bool Socket::Connect(const InetAddress& addr) {
    if (!IsValid()) {
        LOG_ERROR("cannot connect invalid socket");
        return false;
    }
    if (::connect(m_fd, addr.GetSockAddr(), addr.GetSockLen()) != 0 && errno != EINPROGRESS) {
        LOG_ERROR(std::format("connect() failed: {}", std::strerror(errno)));
        return false;
    }
    return true;
}

bool Socket::Close() {
    if (!IsValid()) {
        return false;
    }
    if (::close(m_fd) != 0) {
        LOG_ERROR(std::format("close() failed: {}", std::strerror(errno)));
        m_fd = kInvalidFd;
        return false;
    }
    m_fd = kInvalidFd;
    return true;
}

bool Socket::ShutdownWrite() {
    if (!IsValid()) {
        LOG_ERROR("cannot shutdown invalid socket");
        return false;
    }
    if (::shutdown(m_fd, SHUT_WR) != 0) {
        LOG_ERROR(std::format("shutdown(SHUT_WR) failed: {}", std::strerror(errno)));
        return false;
    }
    return true;
}

bool Socket::SetReuseAddr(bool on) {
    const int value = on ? 1 : 0;
    return SetSocketOption(m_fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value), "SO_REUSEADDR");
}

bool Socket::SetReusePort(bool on) {
    const int value = on ? 1 : 0;
    return SetSocketOption(m_fd, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value), "SO_REUSEPORT");
}

bool Socket::SetKeepAlive(bool on) {
    const int value = on ? 1 : 0;
    return SetSocketOption(m_fd, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value), "SO_KEEPALIVE");
}

bool Socket::SetTcpNoDelay(bool on) {
    const int value = on ? 1 : 0;
    return SetSocketOption(m_fd, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value), "TCP_NODELAY");
}

bool Socket::SetNonBlocking(bool on) {
    if (!IsValid()) {
        LOG_ERROR("cannot set non-blocking on invalid socket");
        return false;
    }

    int flags = ::fcntl(m_fd, F_GETFL, 0);
    if (flags < 0) {
        LOG_ERROR(std::format("fcntl(F_GETFL) failed: {}", std::strerror(errno)));
        return false;
    }

    flags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (::fcntl(m_fd, F_SETFL, flags) != 0) {
        LOG_ERROR(std::format("fcntl(F_SETFL) failed: {}", std::strerror(errno)));
        return false;
    }
    return true;
}

bool Socket::SetCloseOnExec(bool on) {
    if (!IsValid()) {
        LOG_ERROR("cannot set close-on-exec on invalid socket");
        return false;
    }

    int flags = ::fcntl(m_fd, F_GETFD, 0);
    if (flags < 0) {
        LOG_ERROR(std::format("fcntl(F_GETFD) failed: {}", std::strerror(errno)));
        return false;
    }

    flags = on ? (flags | FD_CLOEXEC) : (flags & ~FD_CLOEXEC);
    if (::fcntl(m_fd, F_SETFD, flags) != 0) {
        LOG_ERROR(std::format("fcntl(F_SETFD) failed: {}", std::strerror(errno)));
        return false;
    }
    return true;
}

} // namespace solar_net
