#pragma once

#include "solar_net/base/buffer.h"
#include "solar_net/base/non_copyable.h"
#include "solar_net/base/time.h"
#include "solar_net/net/transport/inet_address.h"
#include "solar_net/net/transport/socket.h"

#include <any>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace solar_net {

class Channel;
class EventLoop;

/**
 * @brief 一条已建立 TCP 连接的管理对象。
 *
 * 聚合 Socket、Channel、输入/输出 Buffer；Send/Shutdown/ForceClose 可跨线程调用。
 * 其余方法须在所属 EventLoop 线程使用。通过 enable_shared_from_this + Channel::SetTie
 * 保证事件处理期间对象存活。
 *
 * 生命周期：kConnecting → ConnectEstablished → kConnected → HandleClose → kDisconnected
 *            → owner 调用 ConnectDestroyed 移除 Channel。
 */
class TcpConnection : public NonCopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, Time)>;
    using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
    using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
    using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;

    TcpConnection(EventLoop* loop,
                  std::string name,
                  int sockfd,
                  const InetAddress& local_addr,
                  const InetAddress& peer_addr);
    ~TcpConnection();

    [[nodiscard]] EventLoop* GetLoop() const noexcept { return m_loop; }
    [[nodiscard]] const std::string& Name() const noexcept { return m_name; }
    [[nodiscard]] const InetAddress& LocalAddress() const noexcept { return m_local_addr; }
    [[nodiscard]] const InetAddress& PeerAddress() const noexcept { return m_peer_addr; }

    [[nodiscard]] bool IsConnected() const noexcept { return m_state == State::kConnected; }
    [[nodiscard]] bool IsDisconnected() const noexcept { return m_state == State::kDisconnected; }

    void SetContext(std::any context);
    [[nodiscard]] const std::any& GetContext() const;

    void SetConnectionCallback(ConnectionCallback cb) { m_connection_callback = std::move(cb); }
    void SetMessageCallback(MessageCallback cb) { m_message_callback = std::move(cb); }
    void SetCloseCallback(CloseCallback cb) { m_close_callback = std::move(cb); }
    void SetWriteCompleteCallback(WriteCompleteCallback cb) { m_write_complete_callback = std::move(cb); }
    void SetHighWaterMarkCallback(HighWaterMarkCallback cb, size_t high_water_mark = 64 * 1024);

    /** @brief 线程安全：非 loop 线程会拷贝数据并 RunInLoop。 */
    void Send(std::string_view message);
    void Send(const void* data, size_t len);
    void Send(Buffer* buffer);

    /** @brief 优雅关闭写端；输出排空后 shutdown(SHUT_WR)。线程安全。 */
    void Shutdown();

    /** @brief 立即关闭连接。线程安全。 */
    void ForceClose();

    /** @brief 连接建立后由 owner 在 loop 线程调用一次。 */
    void ConnectEstablished();

    /** @brief 逻辑移除后由 owner 在 loop 线程调用，从 Poller 移除 Channel。 */
    void ConnectDestroyed();

private:
    enum class State {
        kConnecting,
        kConnected,
        kDisconnecting,
        kDisconnected,
    };

    void SetState(State state) noexcept { m_state = state; }

    void HandleRead(Time receive_time);
    void HandleWrite();
    void HandleClose();
    void HandleError();

    void SendInLoop(std::string_view message);
    void ShutdownInLoop();
    void ForceCloseInLoop();

    [[nodiscard]] std::string StateToString() const;

    EventLoop* m_loop;
    std::string m_name;
    std::atomic<State> m_state{State::kConnecting};

    Socket m_socket;
    std::unique_ptr<Channel> m_channel;

    InetAddress m_local_addr;
    InetAddress m_peer_addr;

    Buffer m_input_buffer;
    Buffer m_output_buffer;

    std::any m_context;
    mutable std::mutex m_context_mutex;

    size_t m_high_water_mark{64 * 1024};

    ConnectionCallback m_connection_callback;
    MessageCallback m_message_callback;
    CloseCallback m_close_callback;
    WriteCompleteCallback m_write_complete_callback;
    HighWaterMarkCallback m_high_water_mark_callback;
};

} // namespace solar_net
