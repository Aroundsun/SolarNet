# Acceptor

`Acceptor` 在监听 socket 上接受 TCP 连接，通过 `NewConnectionCallback` 将 `conn_fd` 交给上层。

头文件：`#include "solar_net/net/transport/acceptor.h"`

## 职责

- 创建 listening `Socket` 与对应 `Channel`。
- `Listen()`：bind + listen + EnableReading。
- 可读时循环 `Accept()`，无回调则立即 close conn_fd。

## 生命周期

```
构造(loop, addr) → SetNewConnectionCallback → Listen()  [须在 loop 线程]
  → HandleRead 循环 accept → 回调(conn_fd, peer)
  → 在 loop 线程 reset/析构（DisableAll + Remove）
```

**重要**：须在所属 `EventLoop` 线程创建、配置、`Listen()` 与销毁；跨线程访问会导致未定义行为。

与 `EventLoopThread` 配合时：

```cpp
std::shared_ptr<Acceptor> acceptor;
loop->RunInLoop([&] {
    acceptor = std::make_shared<Acceptor>(loop, InetAddress(8080));
    acceptor->SetNewConnectionCallback(...);
    acceptor->Listen();
});
// 停止前在 loop 线程销毁
loop->RunInLoop([&] { acceptor.reset(); });
```

## API 要点

```cpp
using NewConnectionCallback = std::function<void(int sockfd, const InetAddress& peer_addr)>;

Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuse_port = true);
void SetNewConnectionCallback(NewConnectionCallback cb);
bool Listen();  // 幂等
```

## 测试 / 示例 / Benchmark

- 测试：`test_acceptor`（6 项）
- 示例：`example_acceptor`
- 基准：`bench_acceptor`

## 下一步

- 与 `EventLoopThreadPool` 结合：主 loop Accept，新连接分配到 worker loop。
- 实现 `TcpConnection` 接管 `conn_fd`。
