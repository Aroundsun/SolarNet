#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace solar_net {

/**
 * @brief IPv4/IPv6 地址值类型，封装 sockaddr_storage。
 *
 * 供 Socket、Acceptor、TcpConnection 使用；可拷贝、可比较，可作为 map 键。
 * 线程安全：只读访问 const 对象可并发。
 */
class InetAddress {
public:
    /** @brief 构造 IPv4 地址 0.0.0.0:0。 */
    InetAddress() noexcept;

    /**
     * @brief 按端口构造地址。
     * @param port 主机字节序端口。
     * @param loopback_only true 时绑定 127.0.0.1 / ::1。
     * @param family AF_INET 或 AF_INET6。
     */
    explicit InetAddress(uint16_t port,
                         bool loopback_only = false,
                         sa_family_t family = AF_INET);

    /**
     * @brief 从 IP 字符串与端口构造。
     *
     * 解析失败时回退为 0.0.0.0:0 并记录错误日志。
     */
    explicit InetAddress(std::string_view ip,
                         uint16_t port,
                         sa_family_t family = AF_INET);

    explicit InetAddress(const sockaddr_in& addr) noexcept;
    explicit InetAddress(const sockaddr_in6& addr) noexcept;
    explicit InetAddress(const sockaddr_storage& addr) noexcept;
    explicit InetAddress(const sockaddr* addr, socklen_t len);

    [[nodiscard]] sa_family_t Family() const noexcept;
    [[nodiscard]] std::string ToIp() const;
    [[nodiscard]] std::string ToIpPort() const;
    [[nodiscard]] uint16_t Port() const noexcept;

    [[nodiscard]] const sockaddr* GetSockAddr() const noexcept;
    [[nodiscard]] sockaddr* GetSockAddr() noexcept;
    [[nodiscard]] socklen_t GetSockLen() const noexcept;

    [[nodiscard]] bool operator==(const InetAddress& other) const noexcept;
    [[nodiscard]] bool operator!=(const InetAddress& other) const noexcept;
    [[nodiscard]] bool operator<(const InetAddress& other) const noexcept;

private:
    void ResetToAny(sa_family_t family = AF_INET) noexcept;

    sockaddr_storage m_addr{};
    socklen_t m_len{sizeof(sockaddr_in)};
};

} // namespace solar_net
