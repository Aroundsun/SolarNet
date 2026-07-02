# TimerQueue

`TimerQueue` 基于 Linux `timerfd` 实现定时器，按到期时间有序维护回调，并与 `EventLoop::Poll` 超时协同。对外 API 通过 `EventLoop::RunAt/RunAfter/RunEvery/Cancel` 暴露。

## 1. 职责

- 管理一次性与周期性定时器。
- 用 `std::set<pair<Time, TimerId>>` 按到期时间排序。
- timerfd 在最早定时器到期时唤醒 EventLoop。
- 无定时器时 disarm timerfd，Poll 可长时间阻塞。

## 2. 类图与生命周期

```
+----------------------------------+
| TimerQueue : NonCopyable         |
+----------------------------------+
| - m_loop: EventLoop*             |
| - m_timer_fd                     |
| - m_timer_channel: Channel       |
| - m_timers: map<id, Entry>       |
| - m_active_timers: set<Time,id>  |
| - m_next_id                      |
+----------------------------------+
| + RunAt / RunAfter / RunEvery    |
| + Cancel(id) -> bool             |
| + NextTimeout() -> int ms        |
| - HandleRead()                   |
| - ProcessExpiredTimers()         |
| - ResetTimerFd(expiration)       |
+----------------------------------+
```

生命周期：

1. **EventLoop 构造**：创建 TimerQueue、timerfd Channel。
2. **RunAfter/RunEvery**：插入 TimerEntry，ResetTimerFd 到最早到期时间。
3. **timerfd 可读**：HandleRead → ProcessExpiredTimers → 执行回调；周期定时器重新插入。
4. **Cancel**：从 map/set 移除，必要时 ResetTimerFd。
5. **析构**：移除 Channel，close timerfd。

## 3. API

```cpp
namespace solar_net {

class TimerQueue : NonCopyable {
 public:
  using TimerCallback = std::function<void()>;
  using TimerId = int64_t;

  explicit TimerQueue(EventLoop* loop);

  TimerId RunAt(Time time, TimerCallback callback);
  TimerId RunAfter(std::chrono::milliseconds delay, TimerCallback callback);
  TimerId RunEvery(std::chrono::milliseconds interval, TimerCallback callback);
  bool Cancel(TimerId id);

  int NextTimeout() const;  // -1 表示无定时器
};

}  // namespace solar_net
```

应用层通常通过 EventLoop 调用：

```cpp
loop.RunAfter(std::chrono::milliseconds(100), [] { /* ... */ });
loop.RunEvery(std::chrono::seconds(1), [] { /* ... */ });
loop.Cancel(id);
```

头文件：`#include "solar_net/net/timer_queue.h"`（一般通过 `event_loop.h` 使用）

## 4. 关键流程

### 到期处理

```
timerfd readable
   | HandleRead()
   | now = Time::Now()
   | ProcessExpiredTimers(now)
   |   while earliest <= now:
   |     run callback
   |     if repeating: re-insert with now+interval
   | ResetTimerFd(next_earliest) or disarm if empty
```

### 与 Poll 协同

```
Loop()
   | timeout_ms = timer_queue->NextTimeout()
   | poller->Poll(timeout_ms, ...)
```

## 5. 设计要点

- **须在 loop 线程调用**：与 Channel 相同，AssertInLoopThread。
- **TimerId**：单调递增 int64，Cancel 无效 id 返回 false。
- **RunEvery**：回调执行后按 interval 重新调度，非 drift-free（与 muduo 一致）。
- **空队列 disarm**：避免 timerfd 无意义唤醒。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| Cancel 无效 id | 返回 false。 |
| RunEvery interval 为 0 | 行为依赖实现，应避免。 |
| 回调内 Cancel 自身 | 需在实现中保证迭代安全（当前在 expired 集合上操作）。 |
| 系统时钟跳变 | 基于 system_clock，大跳变可能影响到期判断。 |

## 7. 测试覆盖

- `RunAfterFiresOnce`：延迟任务执行一次。
- `RunEveryFiresMultipleTimes`：周期任务多次触发。
- `CancelPreventsFiring`：取消后不再执行。
- `CancelInvalidReturnsFalse`：无效 id 返回 false。

## 8. 示例

```cpp
#include "solar_net/net/event_loop.h"

int main() {
    solar_net::EventLoop loop;

    loop.RunAfter(std::chrono::milliseconds(100), [&] {
        // one-shot
    });

    const auto id = loop.RunEvery(std::chrono::milliseconds(50), [&] {
        // repeating
    });

    loop.RunAfter(std::chrono::milliseconds(300), [&] {
        loop.Cancel(id);
        loop.Quit();
    });

    loop.Loop();
    return 0;
}
```

运行：

```bash
./build/examples/example_timer_queue
```

## 9. 性能

- 插入/删除：O(log n)（set + unordered_map）。
- 到期批量处理：O(k) k 为到期数量。
- timerfd 比 thread + sleep 更适合与 epoll 统一调度。

Benchmark：`bench_timer_queue`。

## 10. 下一步

- 已集成进 [EventLoop](event_loop.md)；[EventLoopThread](event_loop_thread.md) 中须 `RunInLoop` 注册定时器。
- TcpConnection 超时、心跳、重连退避将基于 RunAfter/RunEvery 实现。
