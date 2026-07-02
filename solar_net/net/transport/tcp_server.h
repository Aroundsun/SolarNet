#pragma once

#include "solar_net/base/non_copyable.h"
#include "solar_net/net/event_loop_thread_pool.h"
#include "solar_net/net/transport/acceptor.h"
#include "solar_net/net/transport/inet_address.h"
#include "solar_net/net/transport/tcp_connection.h"

#include <atomic>
#include <cstddef>
#include <map>
#include <memory>
#include <string>

namespace solar_net {

class EventLoop;

/**
 * @brief 基于 Acceptor + TcpConnection 的多线程 TCP 服务器。
 *
 * 主 loop 负责 Acceptor 与连接表；可选 EventLoopThreadPool 按 Round-Robin 分配 IO。
 * Start/Stop 线程安全（内部 RunInLoop）；须在 loop 运行期间使用。
 */
class TcpServer : NonCopyable {
public:
    using ConnectionCallback = TcpConnection::ConnectionCallback;
    using MessageCallback = TcpConnection::MessageCallback;
    using WriteCompleteCallback = TcpConnection::WriteCompleteCallback;
    using HighWaterMarkCallback = TcpConnection::HighWaterMarkCallback;

    TcpServer(EventLoop* loop, const InetAddress& listen_addr, std::string name = {});
    ~TcpServer();

    /** @brief 须在 Start() 前调用；0 表示所有 IO 在主 loop。 */
    void SetThreadNum(size_t thread_num);

    void SetConnectionCallback(ConnectionCallback cb) { m_connection_callback = std::move(cb); }
    void SetMessageCallback(MessageCallback cb) { m_message_callback = std::move(cb); }
    void SetWriteCompleteCallback(WriteCompleteCallback cb) { m_write_complete_callback = std::move(cb); }
    void SetHighWaterMarkCallback(HighWaterMarkCallback cb, size_t high_water_mark = 64 * 1024);

    [[nodiscard]] EventLoop* GetLoop() const noexcept { return m_loop; }
    [[nodiscard]] const std::string& Name() const noexcept { return m_name; }
    [[nodiscard]] const InetAddress& ListenAddress() const noexcept { return m_listen_addr; }
    [[nodiscard]] size_t ThreadNum() const noexcept { return m_thread_num; }

    /** @brief 启动线程池（若配置）并开始监听；幂等。 */
    void Start();

    /** @brief 停止监听、关闭所有连接、停止线程池；幂等。 */
    void Stop();

    [[nodiscard]] bool IsRunning() const noexcept { return m_started.load(std::memory_order_acquire); }
    [[nodiscard]] bool IsFullyStopped() const noexcept { return m_stop_complete.load(std::memory_order_acquire); }

private:
    void NewConnection(int sockfd, const InetAddress& peer_addr);
    void RemoveConnection(const TcpConnection::TcpConnectionPtr& conn);
    void RemoveConnectionInLoop(const TcpConnection::TcpConnectionPtr& conn);
    void StartInLoop();
    void StopInLoop();
    void StopThreadPoolInLoop();

    EventLoop* m_loop;
    std::string m_name;
    InetAddress m_listen_addr;
    size_t m_thread_num{0};

    std::unique_ptr<Acceptor> m_acceptor;
    std::unique_ptr<EventLoopThreadPool> m_thread_pool;

    using ConnectionMap = std::map<std::string, TcpConnection::TcpConnectionPtr>;
    ConnectionMap m_connections;

    std::atomic<int> m_next_conn_id{1};
    std::atomic<bool> m_started{false};
    std::atomic<bool> m_stop_complete{true};

    ConnectionCallback m_connection_callback;
    MessageCallback m_message_callback;
    WriteCompleteCallback m_write_complete_callback;
    HighWaterMarkCallback m_high_water_mark_callback;
    size_t m_high_water_mark{64 * 1024};
};

} // namespace solar_net
