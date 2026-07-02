# EventLoop

`EventLoop` 是 SolarNet Reactor 的核心，在单线程中驱动 `Poller` 等待 I/O、处理 `Channel` 回调、执行跨线程任务（pending functors），并集成 `TimerQueue` 定时器。

## 1. 职责

- 运行 `Loop()` 直至 `Quit()`。
- 绑定线程：构造线程即 loop 线程（`IsInLoopThread`）。
- 通过 eventfd 唤醒阻塞的 Poll，处理 `QueueInLoop` 任务。
- 委托 Poller 管理 Channel，委托 TimerQueue 管理定时器。
- 提供 `RunInLoop` / `QueueInLoop` 跨线程调度。

## 2. 类图与生命周期

```
+----------------------------------+
| EventLoop : NonCopyable          |
+----------------------------------+
| - m_thread_id                    |
| - m_poller: unique_ptr<Poller>   |
| - m_wakeup_fd / m_wakeup_channel |
| - m_timer_queue                  |
| - m_pending_functors + m_mutex   |
| - m_looping / m_quit / ...       |
+----------------------------------+
| + Loop() / Quit()                |
| + RunInLoop / QueueInLoop        |
| + UpdateChannel / RemoveChannel  |
| + RunAt / RunAfter / RunEvery    |
| + Cancel / NextTimeout           |
+----------------------------------+
       | uses          | uses
       v               v
+------------+   +------------+
| Poller     |   | TimerQueue |
+------------+   +------------+
```

生命周期：

1. **构造**（须在最终 loop 线程）：创建 Poller、eventfd wakeup Channel、TimerQueue。
2. **`Loop()`**：Poll → HandleEvent → DoPendingFunctors，直到 `m_quit`。
3. **`Quit()`**：置 quit 标志；若在其他线程则 Wakeup。
4. **析构**：移除 wakeup Channel，close eventfd。

## 3. API

```cpp
namespace solar_net {

class EventLoop : NonCopyable {
 public:
  using Functor = std::function<void()>;

  EventLoop();
  ~EventLoop();

  void Loop();
  void Quit();

  bool IsInLoopThread() const noexcept;
  void RunInLoop(Functor cb);
  void QueueInLoop(Functor cb);

  void UpdateChannel(Channel* channel);
  void RemoveChannel(Channel* channel);
  Poller& GetPoller() noexcept;

  TimerQueue::TimerId RunAt(Time time, TimerQueue::TimerCallback cb);
  TimerQueue::TimerId RunAfter(std::chrono::milliseconds delay, TimerQueue::TimerCallback cb);
  TimerQueue::TimerId RunEvery(std::chrono::milliseconds interval, TimerQueue::TimerCallback cb);
  bool Cancel(TimerQueue::TimerId id);

  int NextTimeout() const;
};

}  // namespace solar_net
```

头文件：`#include "solar_net/net/event_loop.h"`

## 4. 关键流程

### 主循环

```
Loop()
   | while !quit:
   |   active = poller->Poll(NextTimeout())
   |   for ch in active: ch->HandleEvent(time)
   |   DoPendingFunctors()
   |   if quit: break
```

### 跨线程 QueueInLoop

```
Other Thread                EventLoop Thread
   | QueueInLoop(cb)              |
   |   lock, push cb              |
   |   Wakeup(eventfd)            |
   |                              | Poll 返回 wakeup
   |                              | DoPendingFunctors()
   |                              |   run cb
```

## 5. 设计要点

- **One Loop Per Thread**：Channel/Timer/Poller API 须在 loop 线程调用（除 QueueInLoop/Quit）。
- **Quit 后仍处理 pending**：`m_quit` 为 true 时跳过 Poll，仍执行 DoPendingFunctors。
- **Poll 超时与定时器统一**：`NextTimeout()` 来自 TimerQueue，无定时器时为 -1 或默认上限。
- **eventfd 唤醒**：避免其他线程 QueueInLoop 时 loop 永久阻塞在 epoll_wait。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| 非 loop 线程调用 RunAt | AssertInLoopThread 失败（debug）。 |
| Quit 后再 Loop | 仍会处理 pending functors 后退出。 |
| 嵌套 RunInLoop | 同线程立即执行，不排队。 |
| Poll 被信号中断 | 由 Poller 实现处理（EINTR 重试）。 |

## 7. 测试覆盖

- `IsInLoopThread`：线程身份正确。
- `RunInLoopFromSameThread`：同线程立即执行。
- `QueueInLoopFromOtherThread`：跨线程任务被执行。
- `QuitFromOtherThread`：其他线程 Quit 能退出 Loop。
- `ChannelIntegration`：Channel + Poll 集成。
- `NextTimeoutReflectsTimers`：有定时器时超时缩短。

## 8. 示例

```cpp
#include "solar_net/net/event_loop.h"

#include <atomic>
#include <thread>

int main() {
    solar_net::EventLoop loop;
    std::atomic<int> counter{0};

    loop.RunInLoop([&] { counter.fetch_add(1); });

    std::thread worker([&] {
        loop.QueueInLoop([&] { counter.fetch_add(1); });
    });

    std::thread stopper([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop.Quit();
    });

    loop.Loop();
    worker.join();
    stopper.join();
    return 0;
}
```

运行：

```bash
./build/examples/example_event_loop
```

## 9. 性能

- 热路径：epoll_wait + 回调；pending 队列在 Poll 后批量执行。
- QueueInLoop：mutex + eventfd 写，适合中低频跨线程调度。

Benchmark：`bench_event_loop`。

## 10. 下一步

- [EventLoopThread](event_loop_thread.md) 在后台线程运行 EventLoop。
- [TimerQueue](timer_queue.md) 已集成；应用通过 RunAfter/RunEvery 使用。
- 实现 Acceptor / TcpConnection，全部注册到 EventLoop。
