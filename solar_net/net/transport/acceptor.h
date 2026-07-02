#pragma once

#include "solar_net/base/non_copyable.h"
#include "solar_net/net/channel.h"
#include "solar_net/net/transport/inet_address.h"
#include "solar_net/net/transport/socket.h"

#include <functional>
#include <memory>

namespace solar_net {

class EventLoop;

/**
 * @brief 在监听 socket 上接受 TCP 连接。
 *
 * 封装 listening Socket 与其 Channel；可读时循环 accept 并通过回调交出 conn_fd。
 * 不管理已建立连接的生命周期（由 TcpConnection 负责）。
 * 须在所属 EventLoop 线程创建、配置并调用 Listen；HandleRead 在 loop 线程执行。
 * 线程安全：否。
 */
class Acceptor : NonCopyable {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress& peer_addr)>;

    /**
     * @param loop 所属 EventLoop，须 outlive Acceptor。
     * @param listen_addr 本地绑定地址。
     * @param reuse_port 是否启用 SO_REUSEPORT。
     */
    Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuse_port = true);
    ~Acceptor();

    void SetNewConnectionCallback(NewConnectionCallback cb);

    /**
     * @brief bind + listen + EnableReading。须在 loop 线程调用，幂等。
     */
    [[nodiscard]] bool Listen();

    [[nodiscard]] bool IsListening() const noexcept { return m_listening; }
    [[nodiscard]] const InetAddress& ListenAddress() const noexcept { return m_listen_addr; }

private:
    void HandleRead();

    EventLoop* m_loop;
    InetAddress m_listen_addr;
    bool m_reuse_port;
    bool m_listening{false};

    Socket m_listen_socket;
    std::unique_ptr<Channel> m_channel;
    NewConnectionCallback m_new_connection_callback;
};

} // namespace solar_net
