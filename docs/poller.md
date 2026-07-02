# Poller

`Poller` 是 IO 多路复用的抽象基类，维护 fd → `Channel*` 映射；Linux 下默认实现为 `EpollPoller`。`EventLoop` 通过 Poller 等待就绪事件并驱动 `Channel::HandleEvent`。

## 1. 职责

- 抽象 `Poll` / `UpdateChannel` / `RemoveChannel` 接口。
- 维护 `ChannelMap`（fd → Channel*）。
- `NewDefaultPoller()` 在 Linux 返回 `EpollPoller`。
- 保证 Channel 操作在所属 EventLoop 线程执行（`AssertInLoopThread`）。

## 2. 类图与生命周期

```
+----------------------------------+
| Poller : NonCopyable             |
+----------------------------------+
| # m_channels: map<int, Channel*> |
| # m_loop: EventLoop*             |
+----------------------------------+
| + Poll(timeout, active*) = 0     |
| + UpdateChannel(ch) = 0          |
| + RemoveChannel(ch) = 0          |
| + HasChannel(ch)                 |
| + NewDefaultPoller(loop)         |
+----------------------------------+
              ^
              | implements
+----------------------------------+
| EpollPoller                      |
+----------------------------------+
| - m_epoll_fd: int                |
| - m_events: vector<epoll_event>  |
+----------------------------------+
| + Poll / UpdateChannel / Remove  |
| - Update(op, channel)            |
+----------------------------------+
```

生命周期：

1. **EventLoop 构造**：`Poller::NewDefaultPoller(this)` 创建 EpollPoller。
2. **Channel Enable**：`UpdateChannel` → epoll_ctl ADD/MOD。
3. **Poll**：阻塞等待，`active_channels` 填充就绪 Channel。
4. **RemoveChannel**：epoll_ctl DEL，从 map 移除。
5. **EpollPoller 析构**：close epoll fd。

## 3. API

```cpp
namespace solar_net {

class Poller : NonCopyable {
 public:
  using ChannelList = std::vector<Channel*>;

  explicit Poller(EventLoop* loop);
  virtual ~Poller();

  virtual Time Poll(int timeout_ms, ChannelList* active_channels) = 0;
  virtual void UpdateChannel(Channel* channel) = 0;
  virtual void RemoveChannel(Channel* channel) = 0;

  bool HasChannel(Channel* channel) const;
  static std::unique_ptr<Poller> NewDefaultPoller(EventLoop* loop);
};

class EpollPoller : public Poller {
 public:
  explicit EpollPoller(EventLoop* loop);
  Time Poll(int timeout_ms, ChannelList* active_channels) override;
  void UpdateChannel(Channel* channel) override;
  void RemoveChannel(Channel* channel) override;
};

}  // namespace solar_net
```

头文件：`#include "solar_net/net/poller.h"` / `#include "solar_net/net/epoll_poller.h"`

## 4. 关键流程

### Poll 循环（EventLoop 内）

```
Loop()
   | timeout = NextTimeout()  // 与 TimerQueue 协同
   | poll_return_time = poller->Poll(timeout, &active)
   | for ch in active:
   |     ch->HandleEvent(poll_return_time)
```

### UpdateChannel 状态机

```
index == -1 且 events != NONE  -> EPOLL_CTL_ADD, index=1
index >= 1 且 events != NONE  -> EPOLL_CTL_MOD
index >= 1 且 events == NONE  -> EPOLL_CTL_DEL, index=-1
```

## 5. 设计要点

- **策略模式**：Poller 抽象便于将来支持 poll/kqueue（当前仅 epoll）。
- **Channel 指针存储在 epoll_data.ptr**：Poll 时 O(1) 找回 Channel。
- **与 TimerQueue 协同**：`Poll` 超时取自 `EventLoop::NextTimeout()`。
- **线程绑定**：所有方法须在 loop 线程调用。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| Poll 无事件 | 返回空 active 列表，时间戳仍为当前 Time。 |
| timeout = -1 | 无限等待（无定时器且无 fd 事件时）。 |
| Remove 不存在的 Channel | assert / 日志。 |
| 同一 fd 重复 Add | 由 index 状态机避免。 |

## 7. 测试覆盖

- `PollReturnsReadableChannel`：pipe 写后 Poll 得到可读 Channel。
- `PollReturnsWritableChannel`：可写事件正确返回。
- `HasChannelTracksRegistration`：注册后 HasChannel 为真。
- `PollWithoutEventsReturnsEmpty`：无事件时空列表。

## 8. 示例

```cpp
#include "solar_net/net/event_loop.h"
#include "solar_net/net/epoll_poller.h"
#include "solar_net/net/channel.h"

solar_net::EventLoop loop;
auto& poller = static_cast<solar_net::EpollPoller&>(loop.GetPoller());

solar_net::Channel channel(&loop, read_fd);
channel.SetReadCallback([&](solar_net::Time) { /* ... */ });
channel.EnableReading();

solar_net::Poller::ChannelList active;
poller.Poll(0, &active);
for (auto* ch : active) {
    ch->HandleEvent(solar_net::Time::Now());
}
```

运行：

```bash
./build/examples/example_poller
```

## 9. 性能

- Poll：O(就绪事件数)，底层 epoll_wait。
- UpdateChannel：O(log n) map + epoll_ctl。

Benchmark：`bench_poller`，测量 Poll/Update 开销。

## 10. 下一步

- [EventLoop](event_loop.md) 封装 Poll 循环，应用代码不直接调用 Poller。
- 多线程场景下每个 loop 独立 EpollPoller（One Loop Per Thread）。
- 后续 TcpConnection 通过 Channel 间接使用 Poller。
