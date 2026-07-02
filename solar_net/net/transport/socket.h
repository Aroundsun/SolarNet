#pragma once

#include "solar_net/base/non_copyable.h"
#include "solar_net/net/transport/inet_address.h"

#include <cstdint>
#include <utility>

namespace solar_net {

/**
 * @brief Linux socket fd 的 RAII 封装。
 *
 * 提供 bind/listen/accept/connect 及常用 setsockopt 薄包装，不含 Buffer 与连接状态。
 * 不可拷贝，可移动；移动后源对象 fd 置为 -1。
 * 线程安全：否，同一实例须串行使用。
 */
class Socket : NonCopyable {
public:
    explicit Socket(int fd = -1) noexcept;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    ~Socket();

    /** @brief 创建 TCP socket（非阻塞 + close-on-exec）。失败返回无效 Socket 并打日志。 */
    [[nodiscard]] static Socket CreateTcp(sa_family_t family = AF_INET);

    /** @brief 创建 UDP socket（非阻塞 + close-on-exec）。 */
    [[nodiscard]] static Socket CreateUdp(sa_family_t family = AF_INET);

    [[nodiscard]] int Fd() const noexcept { return m_fd; }
    [[nodiscard]] bool IsValid() const noexcept { return m_fd >= 0; }

    [[nodiscard]] bool Bind(const InetAddress& addr);
    [[nodiscard]] bool Listen(int backlog = 128);

    /**
     * @brief 接受新连接。
     * @return (conn_fd, peer_addr)；失败时 fd 为 -1。非阻塞 socket 下 EAGAIN 不记 ERROR 日志。
     */
    [[nodiscard]] std::pair<int, InetAddress> Accept();

    [[nodiscard]] bool Connect(const InetAddress& addr);

    [[nodiscard]] bool Close();
    [[nodiscard]] bool ShutdownWrite();

    [[nodiscard]] bool SetReuseAddr(bool on);
    [[nodiscard]] bool SetReusePort(bool on);
    [[nodiscard]] bool SetKeepAlive(bool on);
    [[nodiscard]] bool SetTcpNoDelay(bool on);
    [[nodiscard]] bool SetNonBlocking(bool on);
    [[nodiscard]] bool SetCloseOnExec(bool on);

private:
    int m_fd;
};

} // namespace solar_net
