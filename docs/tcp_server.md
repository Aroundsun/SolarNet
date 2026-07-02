# TcpServer

## 职责

`TcpServer` 是多线程 Reactor 模型中的 TCP 服务器入口。它负责：

- 持有 `Acceptor`，在指定地址监听新连接。
- 管理可选的 `EventLoopThreadPool`，将连接按 Round-Robin 分发到工作线程。
- 维护活跃连接表（`name -> shared_ptr<TcpConnection>`）。
- 把用户注册的回调转发给每个 `TcpConnection`。
- 在关闭时停止监听、强制关闭所有连接、回收线程池。

`TcpServer` 本身不处理字节流，所有业务数据流通过 `TcpConnection` 完成。

## 类图与生命周期

```
                    main thread
                         |
                         v
                   TcpServer (m_loop)
                    /        \
                   v          v
            Acceptor    EventLoopThreadPool
                |              |
                v              v
        NewConnection    worker EventLoop
                |              |
                +-----> TcpConnection
```

- 构造时创建 `Acceptor` 并注册 `NewConnection` 回调。
- `Start()` 在 server loop 中启动线程池（若配置）并开始监听。
- 收到连接后，`NewConnection` 从线程池取下一个 loop，创建 `TcpConnection`，注册回调并调用 `ConnectEstablished`。
- 连接关闭时，`TcpConnection` 的 `CloseCallback` 通知 `TcpServer` 从连接表中移除，再在连接自己的 loop 中调用 `ConnectDestroyed`。
- `Stop()` 在 server loop 中销毁 `Acceptor`、强制关闭所有连接、停止线程池。

## 状态机

连接状态由 `TcpConnection` 维护，`TcpServer` 只跟踪连接表存在性：

| 状态 | 含义 | 转移来源 |
| --- | --- | --- |
| accepting | 正在监听，可接受新连接 | `Start()` 后 |
| stopping | 不再接受新连接，等待连接清理 | `Stop()` 后 |
| stopped | `Acceptor` 已销毁，连接表为空 | 析构完成 |

头文件：`#include "solar_net/net/transport/tcp_server.h"`

## API

```cpp
class TcpServer : public NonCopyable {
public:
    using ConnectionCallback  = TcpConnection::ConnectionCallback;
    using MessageCallback     = TcpConnection::MessageCallback;
    using WriteCompleteCallback = TcpConnection::WriteCompleteCallback;
    using HighWaterMarkCallback = TcpConnection::HighWaterMarkCallback;

    TcpServer(EventLoop* loop, const InetAddress& listen_addr, std::string name = {});
    ~TcpServer();

    void SetThreadNum(size_t thread_num);

    void SetConnectionCallback(ConnectionCallback cb);
    void SetMessageCallback(MessageCallback cb);
    void SetWriteCompleteCallback(WriteCompleteCallback cb);
    void SetHighWaterMarkCallback(HighWaterMarkCallback cb, size_t high_water_mark = 64 * 1024);

    [[nodiscard]] EventLoop* GetLoop() const noexcept;
    [[nodiscard]] const std::string& Name() const noexcept;
    [[nodiscard]] const InetAddress& ListenAddress() const noexcept;
    [[nodiscard]] size_t ThreadNum() const noexcept;

    void Start();
    void Stop();
};
```

## 关键流程

### 1. 启动

```cpp
EventLoop loop;
TcpServer server(&loop, InetAddress(8080), "http");
server.SetThreadNum(4);
server.SetMessageCallback(OnMessage);
server.Start();
loop.Loop();
```

`Start()` 会：

1. 若 `thread_num > 0`，启动 `EventLoopThreadPool`。
2. 调用 `Acceptor::Listen()` 开始监听。
3. 若端口为 `0`，通过 `getsockname` 解析实际端口并更新 `ListenAddress()`。

### 2. 接受新连接

```cpp
void TcpServer::NewConnection(int sockfd, const InetAddress& peer_addr) {
    EventLoop* io_loop = m_thread_pool ? m_thread_pool->GetNextLoop() : m_loop;
    // ... 创建 local_addr、conn_name
    auto conn = std::make_shared<TcpConnection>(io_loop, conn_name, sockfd, local_addr, peer_addr);
    conn->SetConnectionCallback(m_connection_callback);
    conn->SetMessageCallback(m_message_callback);
    // ... 其他回调
    m_connections[conn_name] = conn;
    io_loop->RunInLoop([conn] { conn->ConnectEstablished(); });
}
```

### 3. 连接关闭

```cpp
void TcpServer::RemoveConnection(const TcpConnection::TcpConnectionPtr& conn) {
    m_loop->RunInLoop([this, conn] { RemoveConnectionInLoop(conn); });
}

void TcpServer::RemoveConnectionInLoop(const TcpConnection::TcpConnectionPtr& conn) {
    m_connections.erase(conn->Name());
    conn->GetLoop()->RunInLoop([conn] { conn->ConnectDestroyed(); });
}
```

### 4. 停止

```cpp
void TcpServer::StopInLoop() {
    m_acceptor.reset();                     // 停止监听
    for (auto& conn : m_connections) {      // 先收集所有 shared_ptr
        conn->ForceClose();                  // 触发关闭链路
    }
    if (m_thread_pool) {
        m_thread_pool->Stop();               // 停止工作线程
    }
}
```

## 设计要点

- **连接归属清晰**：`TcpServer` 只负责连接工厂和连接表，`TcpConnection` 负责单个连接的所有 IO 和状态。
- **线程分配简单**：默认 `thread_num = 0`，所有连接与监听共用一个 loop；>0 时按 Round-Robin 分配到 `EventLoopThreadPool`。
- **关闭安全**：`Stop()` 在 server loop 中执行，先停监听，再强制关闭连接，最后停线程池，避免在遍历 map 时修改 map。
- **端口 0 支持**：启动后 `ListenAddress()` 返回内核分配的实际端口，便于测试。
- **回调转发**：用户注册在 `TcpServer` 上的回调会被复制到每个 `TcpConnection`，因此业务回调运行在连接所属 loop 线程。

## 边界情况

| 场景 | 处理 |
| --- | --- |
| `SetThreadNum` 在 `Start()` 后调用 | 记录警告并忽略，避免运行时修改线程模型。 |
| `Start()` 多次调用 | 幂等，第二次及以后直接返回。 |
| `Stop()` 多次调用 | 幂等，第二次及以后直接返回。 |
| 无 `MessageCallback` | 连接仍能建立，但数据到达后不会被消费，需用户自行处理。 |
| 端口被占用 | `Acceptor::Listen()` 返回 false，`Start()` 失败，日志记录错误。 |
| 连接关闭时 server loop 繁忙 | `CloseCallback` 通过 `RunInLoop` 排队到 server loop，保证连接表线程安全。 |

## 测试覆盖

- `StartStop`：多线程启动与停止，验证 `ListenAddress()` 能解析到实际端口。
- `SingleConnectionEcho`：单线程模式下，客户端发送数据，服务器回显，验证 `ConnectionCallback` 与 `MessageCallback`。
- `MultiThreadDistribution`：配置 3 个 worker 线程，6 个客户端连接，验证连接能被建立并分配到不同 loop。

## 示例

```cpp
#include "solar_net/net/event_loop.h"
#include "solar_net/net/transport/tcp_server.h"
#include "solar_net/net/transport/inet_address.h"
#include "solar_net/net/transport/tcp_connection.h"
#include "solar_net/base/buffer.h"

int main() {
    solar_net::EventLoop loop;
    std::thread loop_thread([&] { loop.Loop(); });

    solar_net::TcpServer server(&loop, solar_net::InetAddress(8080), "echo");
    server.SetThreadNum(4);

    server.SetMessageCallback([](const solar_net::TcpConnection::TcpConnectionPtr& conn,
                                 solar_net::Buffer* buf,
                                 solar_net::Time) {
        conn->Send(buf);
    });

    server.Start();
    // ...
    server.Stop();
    loop.Quit();
    loop_thread.join();
    return 0;
}
```

完整可运行版本见 `examples/example_tcp_server.cpp`。

## 性能

- 连接建立开销：一次 `Acceptor` 回调 + `TcpConnection` 构造 + `Channel::SetTie` + `ConnectEstablished`。
- 线程分配：Round-Robin，当前为 O(1) 原子自增；后续可替换为一致性哈希或最小连接数策略。
- 连接表：使用 `std::map<std::string, TcpConnectionPtr>`，查找/删除 O(log N)。连接数通常不大（<10k），足够；若需更高吞吐可替换为 `unordered_map`。

## 下一步

- `Connector`：客户端主动连接模块，与 `TcpServer` 对称，复用 `TcpConnection`。
