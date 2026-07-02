#include "solar_net/net/transport/inet_address.h"

#include "solar_net/base/logger.h"

#include <format>
#include <string>

namespace solar_net {

namespace {

constexpr size_t kIpv6StringLength = INET6_ADDRSTRLEN;

} // namespace

InetAddress::InetAddress() noexcept {
    ResetToAny(AF_INET);
}

InetAddress::InetAddress(uint16_t port, bool loopback_only, sa_family_t family) {
    ResetToAny(family);

    if (family == AF_INET) {
        auto* addr = reinterpret_cast<sockaddr_in*>(&m_addr);
        addr->sin_port = htons(port);
        if (loopback_only) {
            addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        }
    } else if (family == AF_INET6) {
        auto* addr = reinterpret_cast<sockaddr_in6*>(&m_addr);
        addr->sin6_port = htons(port);
        if (loopback_only) {
            addr->sin6_addr = in6addr_loopback;
        }
    }
}

InetAddress::InetAddress(std::string_view ip, uint16_t port, sa_family_t family) {
    ResetToAny(family);

    const std::string ip_str(ip);

    if (family == AF_INET) {
        auto* addr = reinterpret_cast<sockaddr_in*>(&m_addr);
        if (inet_pton(AF_INET, ip_str.c_str(), &addr->sin_addr) != 1) {
            LOG_ERROR(std::format("inet_pton failed for IPv4 address: {}", ip));
            ResetToAny(AF_INET);
        } else {
            addr->sin_port = htons(port);
        }
    } else if (family == AF_INET6) {
        auto* addr = reinterpret_cast<sockaddr_in6*>(&m_addr);
        if (inet_pton(AF_INET6, ip_str.c_str(), &addr->sin6_addr) != 1) {
            LOG_ERROR(std::format("inet_pton failed for IPv6 address: {}", ip));
            ResetToAny(AF_INET6);
        } else {
            addr->sin6_port = htons(port);
        }
    } else {
        LOG_ERROR(std::format("Unsupported address family: {}", family));
    }
}

InetAddress::InetAddress(const sockaddr_in& addr) noexcept : m_len(sizeof(sockaddr_in)) {
    std::memcpy(&m_addr, &addr, sizeof(addr));
}

InetAddress::InetAddress(const sockaddr_in6& addr) noexcept : m_len(sizeof(sockaddr_in6)) {
    std::memcpy(&m_addr, &addr, sizeof(addr));
}

InetAddress::InetAddress(const sockaddr_storage& addr) noexcept : m_len(sizeof(addr)) {
    std::memcpy(&m_addr, &addr, sizeof(addr));
    if (addr.ss_family == AF_INET) {
        m_len = sizeof(sockaddr_in);
    } else if (addr.ss_family == AF_INET6) {
        m_len = sizeof(sockaddr_in6);
    }
}

InetAddress::InetAddress(const sockaddr* addr, socklen_t len) {
    if (addr == nullptr) {
        ResetToAny(AF_INET);
        return;
    }

    if (len > static_cast<socklen_t>(sizeof(m_addr))) {
        len = static_cast<socklen_t>(sizeof(m_addr));
    }

    std::memcpy(&m_addr, addr, len);
    m_len = len;

    if (m_addr.ss_family != AF_INET && m_addr.ss_family != AF_INET6) {
        LOG_ERROR(std::format("InetAddress received unsupported address family: {}", m_addr.ss_family));
        ResetToAny(AF_INET);
    }
}

sa_family_t InetAddress::Family() const noexcept {
    return m_addr.ss_family;
}

std::string InetAddress::ToIp() const {
    char buffer[kIpv6StringLength]{};

    if (Family() == AF_INET) {
        const auto* addr = reinterpret_cast<const sockaddr_in*>(&m_addr);
        inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer));
    } else if (Family() == AF_INET6) {
        const auto* addr = reinterpret_cast<const sockaddr_in6*>(&m_addr);
        inet_ntop(AF_INET6, &addr->sin6_addr, buffer, sizeof(buffer));
    }

    return buffer;
}

std::string InetAddress::ToIpPort() const {
    if (Family() == AF_INET6) {
        return std::format("[{}]:{}", ToIp(), Port());
    }
    return std::format("{}:{}", ToIp(), Port());
}

uint16_t InetAddress::Port() const noexcept {
    if (Family() == AF_INET) {
        return ntohs(reinterpret_cast<const sockaddr_in*>(&m_addr)->sin_port);
    }
    if (Family() == AF_INET6) {
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&m_addr)->sin6_port);
    }
    return 0;
}

const sockaddr* InetAddress::GetSockAddr() const noexcept {
    return reinterpret_cast<const sockaddr*>(&m_addr);
}

sockaddr* InetAddress::GetSockAddr() noexcept {
    return reinterpret_cast<sockaddr*>(&m_addr);
}

socklen_t InetAddress::GetSockLen() const noexcept {
    return m_len;
}

bool InetAddress::operator==(const InetAddress& other) const noexcept {
    return m_len == other.m_len &&
           std::memcmp(&m_addr, &other.m_addr, m_len) == 0;
}

bool InetAddress::operator!=(const InetAddress& other) const noexcept {
    return !(*this == other);
}

bool InetAddress::operator<(const InetAddress& other) const noexcept {
    if (m_addr.ss_family != other.m_addr.ss_family) {
        return m_addr.ss_family < other.m_addr.ss_family;
    }
    return std::memcmp(&m_addr, &other.m_addr, m_len < other.m_len ? m_len : other.m_len) < 0;
}

void InetAddress::ResetToAny(sa_family_t family) noexcept {
    std::memset(&m_addr, 0, sizeof(m_addr));
    m_addr.ss_family = family;

    if (family == AF_INET) {
        m_len = sizeof(sockaddr_in);
        auto* addr = reinterpret_cast<sockaddr_in*>(&m_addr);
        addr->sin_addr.s_addr = INADDR_ANY;
    } else if (family == AF_INET6) {
        m_len = sizeof(sockaddr_in6);
        auto* addr = reinterpret_cast<sockaddr_in6*>(&m_addr);
        addr->sin6_addr = in6addr_any;
    } else {
        m_len = sizeof(sockaddr_storage);
    }
}

} // namespace solar_net
