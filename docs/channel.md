# Channel

`Channel` 是可选择的 I/O 事件通道，连接 `Poller`/`EventLoop` 与上层 IO 对象（如 TcpConnection）。它不拥有 fd，只负责事件注册与回调分发。

## 1. 职责

- 绑定 `EventLoop` 与文件描述符。
- 维护关注事件（读/写）并通过 `EventLoop::UpdateChannel` 同步到 Poller。
- 根据 `revents` 分发读、写、关闭、错误回调。
- 提供 `Tie` 机制，防止 owner 销毁后回调访问悬空对象。

## 2. 类图与生命周期

```
+----------------------------------+
| Channel : NonCopyable            |
+----------------------------------+
| - m_loop: EventLoop*             |
| - m_fd: int                      |
| - m_events / m_revents           |
| - m_index: int                   |
| - m_tie: weak_ptr<void>          |
| - m_*_callback                   |
+----------------------------------+
| + EnableReading/Writing          |
| + DisableAll / Remove            |
| + HandleEvent(Time)              |
| + SetTie(shared_ptr)             |
+----------------------------------+
              |
              | registered in
              v
+----------------------------------+
| Poller (fd -> Channel*)          |
+----------------------------------+
```

生命周期：

1. **构造**：绑定 loop 与 fd，`index = -1`（未注册）。
2. **EnableReading/Writing**：设置 `m_events`，调用 `Update()` → Poller 注册/更新。
3. **Poll 返回**：`SetRevents` → `HandleEvent` 触发回调。
4. **Remove**：从 Poller 移除，要求已无关注事件且 `index == -1` 流程完成。
5. **析构**：须已 DisableAll 且 Remove，否则 assert。

## 3. API

```cpp
namespace solar_net {

class Channel : NonCopyable {
 public:
  using EventCallback = std::function<void()>;
  using ReadEventCallback = std::function<void(Time)>;

  Channel(EventLoop* loop, int fd);
  ~Channel();

  int Fd() const noexcept;
  void EnableReading();
  void EnableWriting();
  void DisableAll();
  void Remove();

  void SetRevents(int revents) noexcept;
  void HandleEvent(Time receive_time);

  void SetReadCallback(ReadEventCallback cb);
  void SetWriteCallback(EventCallback cb);
  void SetCloseCallback(EventCallback cb);
  void SetErrorCallback(EventCallback cb);
  void SetTie(const std::shared_ptr<void>& obj);

  static const int kNoneEvent;
  static const int kReadEvent;   // POLLIN | POLLPRI
  static const int kWriteEvent;  // POLLOUT
};

}  // namespace solar_net
```

头文件：`#include "solar_net/net/channel.h"`

## 4. 关键流程

### 事件分发

```
HandleEvent(receive_time)
   | Tie 检查 owner 是否存活
   | POLLIN | POLLPRI  -> read_callback(receive_time)
   | POLLOUT            -> write_callback()
   | POLLHUP            -> close_callback()（无 POLLIN 时）
   | POLLERR            -> error_callback()
```

### 注册到 Poller

```
EnableReading()
   | m_events |= kReadEvent
   | Update() -> loop->UpdateChannel(this)
   |            -> poller->UpdateChannel(this)
```

## 5. 设计要点

- **One Loop Per Thread**：Channel 只能在所属 EventLoop 线程操作。
- **不拥有 fd**：fd 由 TcpConnection 或 EventLoop 内部（eventfd/timerfd）管理。
- **index 状态机**：`-1` 未添加，`1` 已添加，`2` 待删除（Poller 内部使用）。
- **Tie + weak_ptr**：模仿 muduo，避免 TcpConnection 销毁后仍触发回调。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| HUP 且有 IN | 先读回调，再 close（若有）。 |
| HUP 且无 IN | 直接 close 回调。 |
| Tie 对象已销毁 | HandleEvent 跳过所有回调。 |
| Remove 时仍有 events | Poller/EventLoop 侧 assert 或拒绝。 |
| fd = -1 | 构造允许，实际使用前应绑定有效 fd。 |

## 7. 测试覆盖

- `ConstructWithFdAndLoop` / `EnableReading` / `EnableWriting` / `ReadingAndWritingCanCoexist`
- `HandleEventInvokesReadCallback` / `HandleEventInvokesWriteCallback`
- `HandleEventInvokesCloseCallbackOnHupWithoutIn` / `HandleEventInvokesReadBeforeCloseWhenHupWithIn`
- `HandleEventInvokesErrorCallbackOnPollErr`
- `TiePreventsCallbackAfterOwnerDestroyed` / `TieAllowsCallbackWhenOwnerAlive`
- `RemoveResetsIndex`

## 8. 示例

```cpp
#include "solar_net/net/channel.h"
#include "solar_net/net/event_loop.h"

solar_net::EventLoop loop;
solar_net::Channel channel(&loop, fd);

channel.SetReadCallback([&](solar_net::Time t) { /* ... */ });
channel.EnableReading();

// Poll 返回后：
channel.SetRevents(POLLIN);
channel.HandleEvent(solar_net::Time::Now());
```

运行：

```bash
./build/examples/example_channel
# read=1 write=1 close=1 error=1
```

## 9. 性能

- Enable/Update：一次 epoll_ctl，属于连接建立期操作。
- HandleEvent：O(1) 回调分发，热路径在 read/write 回调内。

Benchmark：`bench_channel`，测量 Enable/HandleEvent 开销。

## 10. 下一步

- 与 [EventLoop](event_loop.md) 集成，由 `Loop()` 统一 Poll + HandleEvent。
- 实现 `TcpConnection`，内部持有 Channel + Buffer。
- Acceptor 在新连接上创建 Channel 并注册读事件。
