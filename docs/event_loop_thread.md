# EventLoopThread

`EventLoopThread` 在独立线程中创建并运行一个 `EventLoop`，并向调用者暴露 `EventLoop*` 指针。它是 SolarNet 多线程 Reactor 的基础单元，被 `EventLoopThreadPool` 复用，实现 **One Loop Per Thread**。

## 1. 职责

- 拥有一条后台线程。
- 在线程栈上构造 `EventLoop`。
- 通过 `mutex + condition_variable` 把 `EventLoop*` 同步给调用者。
- 提供 `InitCallback`，允许在 `EventLoop::Loop()` 启动前在线程内完成初始化。
- 析构时自动停止并 Join 线程，防止泄漏。

## 2. 类图与生命周期

```
+---------------------------------+
| EventLoopThread                 |
+---------------------------------+
| - m_name: string                |
| - m_initCallback: function      |
| - m_mutex: mutex                |
| - m_cond: condition_variable    |
| - m_loop: EventLoop*            |
| - m_thread: unique_ptr<Thread>  |
| - m_started: bool               |
| - m_stopped: bool               |
+---------------------------------+
| + Start() -> EventLoop*         |
| + Stop()                        |
| + GetLoop() -> EventLoop*       |
| + GetName() -> const string&    |
| - ThreadFunc()                  |
+---------------------------------+
              |
              | owns
              v
+---------------------------------+
| Thread                          |
+---------------------------------+
| + Start()                       |
| + Join()                        |
+---------------------------------+
```

生命周期：

1. **构造**：保存线程名和初始化回调，不创建线程。
2. **`Start()`**：创建 `Thread` 并启动，阻塞等待 `m_loop` 就绪，返回 `EventLoop*`。
3. **`ThreadFunc()`**：栈上构造 `EventLoop`，设置 `m_loop` 并执行 `InitCallback`，调用 `loop.Loop()`，退出后清空 `m_loop`。
4. **`Stop()` / 析构**：向 loop 发送 `Quit()`，显式 `Join()` 后台线程，清空指针。
5. **重复 `Start()`**：直接返回已有 `EventLoop*`，不创建新线程。

## 3. API

```cpp
namespace solar_net {

class EventLoopThread : NonCopyable {
 public:
  using InitCallback = std::function<void(EventLoop*)>;

  explicit EventLoopThread(std::string name = {}, InitCallback callback = {});
  ~EventLoopThread();

  EventLoopThread(EventLoopThread&&) = delete;
  EventLoopThread& operator=(EventLoopThread&&) = delete;

  EventLoop* Start();
  void Stop();

  EventLoop* GetLoop() const;
  const std::string& GetName() const noexcept;
};

}  // namespace solar_net
```

头文件：`#include "solar_net/net/event_loop_thread.h"`

## 4. 关键流程

### 启动流程

```
Main Thread                EventLoopThread               Thread
   |                              |                         |
   | Start()                      |                         |
   | ---------------------------->|                         |
   |   create Thread + Start()    |                         |
   |   wait m_cond                |                         |
   |                              | ThreadFunc()            |
   |                              |   EventLoop loop;       |
   |                              |   lock -> m_loop = &loop |
   |                              |   initCallback(&loop)   |
   |                              |   notify                |
   |   return m_loop              |<------------------------|
   |<---------------------------- |                         |
   |                              |   loop.Loop()           |
```

### 停止流程

```
Main Thread
   | Stop()
   |   m_stopped = true
   |   loop->RunInLoop(Quit)  // 或 loop 线程内直接 Quit()
   |   thread->Join()
   |   m_loop = nullptr
   |<-- 线程退出，EventLoop 栈对象销毁
```

## 5. 设计要点

- **非移动**：线程函数捕获 `this`，移动后 `this` 失效，因此显式删除移动构造和赋值。
- **不拥有 EventLoop**：`EventLoop` 是线程栈对象，`EventLoopThread` 只持有指针，保证生命周期清晰。
- **复用 Thread**：通过 `std::unique_ptr<Thread>` 管理，停止时显式 `Join()` 后再释放。
- **异常安全**：`InitCallback` 中捕获异常并记录日志，避免未处理异常导致 `std::terminate`。
- **自停保护**：若 `Stop()` 在 loop 线程内调用，直接 `Quit()` 而不在该线程中 Join 自己，避免死锁。
- **跨线程任务**：调用者通过 `EventLoop::RunInLoop()` / `QueueInLoop()` 向 loop 投递任务；定时器 API 须在 loop 线程内注册。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| 未 Start 就析构 | `Stop()` 检查 `m_started`，直接返回。 |
| 多次 Start | 第二次直接返回同一 `EventLoop*`，不创建新线程。 |
| 多次 Stop | 第二次直接返回，避免重复 Join。 |
| loop 线程内调用 Quit | `Loop()` 正常返回，`ThreadFunc` 清空 `m_loop`。 |
| 线程名空 | 底层 `Thread` 使用默认名。 |

## 7. 测试覆盖

- `StartReturnsLoop`：启动后返回非空 `EventLoop*`，与 `GetLoop()` 一致。
- `InitCallbackRunsInLoopThread`：初始化回调在 loop 线程中执行，并拿到正确指针。
- `ThreadNameIsSet`：通过 `InitCallback` 验证线程名被正确设置。
- `RunInLoopFromOtherThread`：从其他线程提交任务，loop 线程中执行。
- `MultipleStartReturnsSameLoop`：重复 `Start()` 返回同一指针。
- `StopIsIdempotent`：多次 `Stop()` 不崩溃。
- `LoopCanQuitItself`：loop 线程内定时器触发 `Quit()` 后，线程能正常退出。
- `DestructorStopsRunningThread`：析构自动停止并 Join 运行中的线程。

## 8. 示例

```cpp
#include "solar_net/net/event_loop_thread.h"
#include "solar_net/net/event_loop.h"

#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <thread>

int main() {
    solar_net::EventLoopThread loop_thread("example");
    solar_net::EventLoop* loop = loop_thread.Start();
    if (loop == nullptr) {
        return 1;
    }

    std::atomic<int> counter{0};

    // 定时器须在 loop 线程内注册
    loop->RunInLoop([&] {
        loop->RunEvery(std::chrono::milliseconds(500), [&] {
            std::cout << std::format("tick {} from {}\n",
                                     counter.fetch_add(1, std::memory_order_relaxed),
                                     loop_thread.GetName());
        });
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    loop_thread.Stop();
    return 0;
}
```

运行：

```bash
./build/examples/example_event_loop_thread
```

## 9. 性能

- 启动一次：一次线程创建 + 一次 `mutex/cond` 唤醒，属于低频操作。
- 运行时无共享锁：主线程通过 `EventLoop*` 调用 `RunInLoop`，无需 `EventLoopThread` 锁。
- 停止时：一次跨线程 `eventfd` 唤醒 + 线程 Join。

Benchmark：`bench_event_loop_thread`，测量 `Start/Stop` 周期开销。

## 10. 下一步

`EventLoopThread` 被 `EventLoopThreadPool` 聚合，按 Round-Robin 提供多个 `EventLoop*`。后续将与 `Acceptor` / `TcpConnection` 结合：

- 主线程 `EventLoop` 负责 `Acceptor`。
- 新连接到来时调用 `pool.GetNextLoop()`，将连接分配到某个工作线程。

详见 [EventLoopThreadPool](event_loop_thread_pool.md)。
