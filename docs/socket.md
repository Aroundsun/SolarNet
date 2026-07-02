# Socket

`Socket` 是 Linux socket fd 的 RAII 封装，提供 bind/listen/accept/connect 与常用 `setsockopt`。

头文件：`#include "solar_net/net/transport/socket.h"`

## 职责

- 拥有 fd，析构/移动时自动 close。
- `CreateTcp` / `CreateUdp` 默认创建非阻塞 + close-on-exec socket。
- 不含 Buffer 与连接状态，供 Acceptor / TcpConnection 复用。

## API 要点

```cpp
static Socket CreateTcp(sa_family_t family = AF_INET);
bool Bind(const InetAddress& addr);
bool Listen(int backlog = 128);
std::pair<int, InetAddress> Accept();  // 非阻塞，EAGAIN 不记 ERROR
bool Connect(const InetAddress& addr); // 非阻塞返回 EINPROGRESS 视为成功
bool SetReuseAddr/SetReusePort/SetTcpNoDelay/...();
```

## 设计要点

- **可移动不可拷贝**，移动后源 fd 为 -1。
- **Accept 返回裸 fd**：上层（Acceptor）负责交给 TcpConnection 或 close。
- **错误日志**：使用 `strerror(errno)`；accept 的 EAGAIN/EWOULDBLOCK 不打 ERROR。

## 测试 / 示例 / Benchmark

- 测试：`test_socket`（9 项）
- 示例：`example_socket`
- 基准：`bench_socket`

## 下一步

被 [Acceptor](acceptor.md) 用作 listening socket；TcpConnection 将持有 connected socket fd。
