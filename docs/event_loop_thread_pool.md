# EventLoopThreadPool

`EventLoopThreadPool` 管理一组 `EventLoopThread`，向调用者提供多个 `EventLoop*`，用于后续 TCP/Acceptor 的多线程分发。它是 SolarNet 多线程 Reactor 的调度层。

## 1. 职责

- 根据指定线程数创建 `EventLoopThread`。
- 启动所有线程，并收集 `EventLoop*` 指针。
- 提供 `GetNextLoop()`，按 **Round-Robin** 轮询分配负载。
- 统一停止并 Join 所有线程。
- 不参与 TCP/Acceptor 逻辑，只负责 Loop 资源池化。

## 2. 类图与生命周期

```
+----------------------------------+
| EventLoopThreadPool              |
+----------------------------------+
| - m_threadCount: size_t          |
| - m_name: string                 |
| - m_initCallback: function       |
| - m_threads: vector<unique_ptr<  |
|              EventLoopThread>>    |
| - m_loops: vector<EventLoop*>     |
| - m_nextIndex: atomic<size_t>    |
| - m_started: atomic<bool>        |
| - m_stopped: atomic<bool>        |
+----------------------------------+
| + Start()                        |
| + Stop()                         |
| + GetNextLoop() -> EventLoop*    |
| + GetLoop(index) -> EventLoop*   |
| + ThreadCount() -> size_t        |
| + IsRunning() -> bool            |
+----------------------------------+
              | owns N
              v
+----------------------------------+
| EventLoopThread                  |
+----------------------------------+
| + Start() -> EventLoop*          |
| + Stop()                         |
+----------------------------------+
```

生命周期：

1. **构造**：保存线程数、池名、初始化回调；不创建线程。
2. **`Start()`**：创建 N 个 `EventLoopThread`（名为 `name-N`），依次启动并收集 `EventLoop*`。
3. **`GetNextLoop()`**：原子自增索引，按模取下一个 `EventLoop*`。
4. **`Stop()`**：遍历所有线程，逐个 `Stop()`（内部 Join）。
5. **析构**：若未 `Stop()`，自动调用。

## 3. API

```cpp
namespace solar_net {

class EventLoopThreadPool : public NonCopyable {
 public:
  using ThreadInitCallback = EventLoopThread::InitCallback;

  explicit EventLoopThreadPool(size_t thread_count = std::thread::hardware_concurrency(),
                               std::string name = {},
                               ThreadInitCallback callback = {});
  ~EventLoopThreadPool() override;

  void Start();
  void Stop();

  EventLoop* GetNextLoop();
  EventLoop* GetLoop(size_t index) const;

  size_t ThreadCount() const noexcept;
  bool IsRunning() const noexcept;
  const std::string& GetName() const noexcept;
};

}  // namespace solar_net
```

## 4. 关键流程

### 启动流程

```
Main Thread
   | Start()
   |   for i in 0..threadCount-1:
   |     create EventLoopThread(name + "-" + i)
   |     loop = thread->Start()
   |     m_loops.push_back(loop)
   |   m_started = true
   |<-- all loops ready
```

### 分配流程

```
Caller
   | GetNextLoop()
   |   idx = m_nextIndex++
   |   return m_loops[idx % m_loops.size()]
   |<-- EventLoop*
```

### 停止流程

```
Main Thread
   | Stop()
   |   m_stopped = true
   |   for each thread in m_threads:
   |     thread->Stop()
   |<-- all threads joined
```

## 5. 设计要点

- **复用 EventLoopThread**：每个线程的创建、就绪同步、停止 Join 都由 `EventLoopThread` 处理，池只负责聚合和调度。
- **Round-Robin 无锁**：`m_nextIndex` 为 `std::atomic<size_t>`，`GetNextLoop()` 仅一次原子自增 + 取模。
- **线程数容错**：显式传入 0 会被当作 1（与 `ThreadPool` 行为一致），因为当前没有 base loop 可回退。
- **非移动**：池内部持有 `unique_ptr<EventLoopThread>`，对象本身不可移动（类似 `ThreadPool`）。
- **InitCallback 透传**：每个 `EventLoopThread` 都会执行相同的 `InitCallback`，便于统一设置线程名、日志级别等。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| thread_count = 0 | 当作 1 处理，保证至少有一个可用 loop。 |
| 未 Start 就调用 GetNextLoop | 返回 `nullptr`。 |
| 未 Start 就析构 | `Stop()` 中检查 `m_started`，无泄漏。 |
| 多次 Start | 第二次直接返回，不创建新线程。 |
| 多次 Stop | 第二次直接返回，避免重复 Join。 |
| GetNextLoop 并发调用 | 原子自增保证线程安全。 |
| 部分线程启动失败 | 当前实现记录错误日志并跳过，实际可用线程数可能少于预期。 |

## 7. 测试覆盖

- `StartReturnsAllLoops`：启动后所有 loop 非空且位于各自线程。
- `GetNextLoopRoundRobin`：多次调用按 0,1,2,...,0,1,2 轮询。
- `GetLoopByIndex`：按索引访问与越界处理。
- `IsRunningState`：启动前后运行状态变化。
- `ZeroThreadCountTreatedAsOne`：0 被规范化为 1。
- `MultipleStartNoEffect`：重复 `Start()` 不创建新线程。
- `StopIsIdempotent`：多次 `Stop()` 不崩溃。
- `GetNextLoopBeforeStartReturnsNull` / `AfterStopReturnsNull`：未启动/停止后返回空。
- `InitCallbackCalledForEachThread`：每个线程都执行初始化回调。
- `ConcurrentGetNextLoop`：多线程并发获取 loop 无数据竞争。

## 8. 示例

```cpp
#include "solar_net/event_loop_thread_pool.h"

#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <thread>

int main() {
    solar_net::EventLoopThreadPool pool(3, "worker");
    pool.Start();

    std::atomic<int> counter{0};

    for (size_t i = 0; i < pool.ThreadCount(); ++i) {
        solar_net::EventLoop* loop = pool.GetLoop(i);
        if (loop == nullptr) {
            continue;
        }

        loop->RunAfter(std::chrono::milliseconds(static_cast<int>(i) * 100), [loop, &counter] {
            loop->RunEvery(std::chrono::milliseconds(500), [&] {
                std::cout << std::format("tick {}\n",
                                         counter.fetch_add(1, std::memory_order_relaxed));
            });
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    pool.Stop();
    return 0;
}
```

## 9. 性能

- 启动：O(N) 线程创建 + N 次同步唤醒，一次性。
- `GetNextLoop()`：O(1) 无锁。
- 停止：O(N) `eventfd` 唤醒 + Join。
- 后续可扩展：一致性哈希、最小连接数、按 CPU 亲和性分配等。

## 10. 下一步

将 `EventLoopThreadPool` 与 `Acceptor` / `TcpConnection` 结合，实现多线程 Reactor：
- 主线程 `EventLoop` 负责 `Acceptor`。
- 新连接到来时调用 `pool.GetNextLoop()`，将连接分配到某个工作线程。
