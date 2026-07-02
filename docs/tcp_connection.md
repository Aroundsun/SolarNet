# TcpConnection

## 1. 职责

`TcpConnection` 管理一条已建立的 TCP 连接，是 SolarNet Transport Layer 的核心对象。它聚合：

- 一个 `Socket`：RAII 持有已连接的 fd；
- 一个 `Channel`：注册到 `EventLoop`，负责可读/可写/错误/关闭事件的分发；
- 两个 `Buffer`：分别缓存从对端读入的字节（`m_input_buffer`）和待发送到对端的字节（`m_output_buffer`）。

`TcpConnection` 提供线程安全的 `Send()`、`Shutdown()`、`ForceClose()`，其余方法原则上在所属 `EventLoop` 线程调用。它继承 `std::enable_shared_from_this<TcpConnection>`，并把 `shared_from_this()` 绑定到 `Channel` 的 tie，确保事件处理期间连接对象不会意外析构。

## 2. 类图与生命周期

```text
EventLoop *           Socket              Channel            Buffer x2
    |                    |                    |                  |
    +--------------------+--------------------+------------------>|
                         TcpConnection
                         - m_name
                         - m_state: Connecting | Connected | Disconnecting | Disconnected
                         - m_context (std::any, optional)
                         - m_high_water_mark
                         - callbacks
```

状态机：

```text
Connecting -> Connected -> Disconnecting -> Disconnected
                |                              ^
                +------------------------------+
                       (ForceClose / peer close / error)
```

生命周期由 owner（通常是 `TcpServer`）控制：

1. 构造时状态为 `kConnecting`；
2. owner 在 loop 线程调用 `ConnectEstablished()`，状态变为 `kConnected`，启用读事件，触发 `ConnectionCallback`；
3. 数据通过 `MessageCallback` 流入业务层，通过 `Send()` 流出；
4. 当对端关闭、主动 `Shutdown()` 完成、或 `ForceClose()` 时，`HandleClose()` 将状态置为 `kDisconnected` 并 DisableAll，触发 `CloseCallback` 和 `ConnectionCallback`；
5. owner 在 loop 线程调用 `ConnectDestroyed()`，从 Poller 移除 Channel。

头文件：`#include "solar_net/net/transport/tcp_connection.h"`

```cpp
namespace solar_net {

class TcpConnection : public NonCopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
    using TcpConnectionPtr    = std::shared_ptr<TcpConnection>;
    using ConnectionCallback  = std::function<void(const TcpConnectionPtr&)>;
    using MessageCallback     = std::function<void(const TcpConnectionPtr&, Buffer*, Time)>;
    using CloseCallback       = std::function<void(const TcpConnectionPtr&)>;
    using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
    using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;

    TcpConnection(EventLoop* loop,
                  std::string name,
                  int sockfd,
                  const InetAddress& local_addr,
                  const InetAddress& peer_addr);
    ~TcpConnection() override;

    EventLoop* GetLoop() const noexcept;
    const std::string& Name() const noexcept;
    const InetAddress& LocalAddress() const noexcept;
    const InetAddress& PeerAddress() const noexcept;

    bool IsConnected() const noexcept;
    bool IsDisconnected() const noexcept;

    void SetContext(std::any context);
    const std::any& GetContext() const;

    void SetConnectionCallback(ConnectionCallback cb);
    void SetMessageCallback(MessageCallback cb);
    void SetCloseCallback(CloseCallback cb);
    void SetWriteCompleteCallback(WriteCompleteCallback cb);
    void SetHighWaterMarkCallback(HighWaterMarkCallback cb, size_t high_water_mark = 64 * 1024);

    void Send(std::string_view message);   // 线程安全
    void Send(const void* data, size_t len); // 线程安全
    void Send(Buffer* buffer);              // 线程安全，取走 buffer 内容

    void Shutdown();        // 优雅关闭写端，线程安全
    void ForceClose();      // 立即关闭，线程安全

    void ConnectEstablished();  // 仅 loop 线程
    void ConnectDestroyed();    // 仅 loop 线程
};

} // namespace solar_net
```

## 4. 关键流程

### 4.1 数据接收

1. `Channel` 报告可读事件；
2. `HandleRead()` 调用 `Buffer::ReadFd()` 把数据读入 `m_input_buffer`；
3. 若读到 `0` 字节，说明对端关闭，进入 `HandleClose()`；
4. 否则触发 `MessageCallback(conn, &m_input_buffer, receive_time)`，由业务层决定消费多少数据。

### 4.2 数据发送

1. 用户调用 `Send()`；
2. 若不在 loop 线程，将数据拷贝到 `std::string` 并通过 `RunInLoop()` 投递；
3. `SendInLoop()` 先尝试直接 `::write()`；
4. 若一次性写完，触发 `WriteCompleteCallback`；
5. 若未写完，把剩余数据追加到 `m_output_buffer`，启用写事件；
6. 当 socket 可写时，`HandleWrite()` 继续 `WriteFd()`，直到 `m_output_buffer` 排空；
7. 写空后禁用写事件，触发 `WriteCompleteCallback`；若当前状态为 `kDisconnecting`，继续调用 `ShutdownInLoop()` 完成半关闭。

### 4.3 连接关闭

1. `HandleClose()` 把状态切到 `kDisconnected`；
2. `Channel::DisableAll()` + `Channel::Remove()`，从 Poller 移除；
3. 持有一个 `shared_ptr` 局部 guard，防止回调链中对象被提前析构；
4. 依次触发 `CloseCallback`（通知 owner 从连接池中移除）和 `ConnectionCallback`（通知用户连接已断开）；
5. owner 随后调用 `ConnectDestroyed()`，完成生命周期收尾。

## 5. 设计要点

- **RAII 与所有权**：`Socket` 独占 fd，`Channel` 不拥有 fd。`TcpConnection` 析构时保证 `Channel` 已从 Poller 移除且 fd 已关闭。
- **线程安全**：`Send/Shutdown/ForceClose` 可通过 `RunInLoop()` 安全地从其他线程调用。内部状态 `m_state` 使用 `std::atomic` 以支持 `IsConnected/IsDisconnected` 的跨线程读取。
- **零拷贝尝试**：`SendInLoop()` 优先尝试直接 `write()`，只有高并发或网络拥塞时才把数据写入 `m_output_buffer`，避免不必要的数据拷贝。
- **高水位保护**：`m_output_buffer` 堆积超过 `m_high_water_mark` 时触发 `HighWaterMarkCallback`，提示业务层暂停发送或限流。
- **半关闭支持**：`Shutdown()` 先设置 `kDisconnecting`，等待输出缓冲区排空后再调用 `Socket::ShutdownWrite()`，符合 TCP 优雅关闭语义。
- **异常安全**：`HandleRead`/`HandleWrite` 遇到 `EAGAIN/EWOULDBLOCK` 不视为错误；`EPIPE/ECONNRESET` 记录日志但等待 Poller 报告最终关闭，避免在 write 路径上做草率关闭决策。

## 6. 边界情况

| 场景 | 处理 |
|------|------|
| `Send()` 在连接已断开时调用 | 丢弃数据并记录 `LOG_WARN` |
| 用户从 loop 线程调用 `Send()` | 直接走 `SendInLoop()`，无需额外拷贝 |
| 高并发下 `m_output_buffer` 堆积 | 触发 `HighWaterMarkCallback`，继续写入 |
| 对端发送 RST | epoll 报告 `POLLERR`/`POLLHUP`，`HandleError` 读取 `SO_ERROR`，随后 `HandleClose` 清理 |
| `ForceClose()` 在 loop 线程调用 | 直接 `HandleClose()`；跨线程则投递到 loop 线程 |
| `ConnectDestroyed()` 被调用两次 | 已断开时仅在有 Channel 注册时 Remove，幂等 |
| 析构时 `Channel` 未移除 | `assert` 保护，确保生命周期被正确管理 |

## 7. 测试覆盖

- 初始状态：`kConnecting`、名字、地址访问器；
- `ConnectEstablished` / `ConnectDestroyed` 状态切换与回调；
- loop 线程内直接发送；
- 跨线程 `Send()`；
- 完整回声（echo）round-trip；
- 对端关闭（`socketpair` 一端 `close`）触发 `CloseCallback`；
- 高水位回调注册与触发；
- `std::any` context 的读写。

## 8. 示例

见 `examples/example_tcp_connection.cpp`：一个基于 `Acceptor + TcpConnection` 的命令行回声服务器，监听 `12345` 端口，把收到的数据原样返回。

```bash
./build/examples/example_tcp_connection
# 在另一个终端
nc localhost 12345
hello
hello
```

## 9. 性能

- 小数据包优先直接 `write()`，避免写入输出缓冲区；
- 输出缓冲区只在 `write()` 未一次写完或内核发送窗口不足时启用，启用写事件等待 POLLOUT；
- 输入缓冲区使用 `Buffer::ReadFd()`，内部利用 `readv` 或单块读入，避免多次拷贝；
- 跨线程发送时 unavoidably 需要一次数据拷贝到 `std::string`；
- 高水位机制防止内存无界增长。

Benchmark：`benchmarks/bench_tcp_connection.cpp` 测量 `SendSmall` 与 `EchoRoundTrip` 两类场景。

## 10. 下一步

- `TcpServer`：组合 `Acceptor`、`EventLoopThreadPool` 与 `TcpConnection` 管理连接池；
- `Connector`：客户端主动连接，处理 `EINPROGRESS` 与可写事件，完成 `TcpConnection` 创建；
- 可选：TLS wrapper、流量统计、空闲检测（keep-alive timer）。
