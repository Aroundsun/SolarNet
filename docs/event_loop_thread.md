# EventLoopThread

`EventLoopThread` 在独立线程中创建并运行一个 `EventLoop`，并向调用者暴露 `EventLoop*` 指针。它是 SolarNet 多线程 Reactor 的基础单元，后续将被 `EventLoopThreadPool` 复用，实现 **One Loop Per Thread**。

## 1. 职责

- 拥有一条后台线程。
- 在线程栈上构造 `EventLoop`。
- 通过 `mutex + condition_variable` 把 `EventLoop*` 同步给调用者。
- 提供 `InitCallback`，允许在 `EventLoop::Loop()` 启动前在线程内完成初始化。
- 析构时自动停止并 Join 线程，防止泄漏。

## 2. 类图

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
| + Start()                       |
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

## 3. API

```cpp
namespace solar_net {

class EventLoopThread : public NonCopyable {
 public:
  using InitCallback = std::function<void(EventLoop*)>;

  explicit EventLoopThread(std::string name = {}, InitCallback callback = {});
  ~EventLoopThread() override;

  EventLoopThread(EventLoopThread&&) = delete;
  EventLoopThread& operator=(EventLoopThread&&) = delete;

  void Start();
  EventLoop* GetLoop() const;
  void Stop();

  const std::string& GetName() const noexcept;
};

} // namespace solar_net
```

## 4. 生命周期

1. **构造**：保存线程名和初始化回调，不创建线程。
2. **`Start()`**：
   - 创建 `Thread`（lambda 捕获 `this` -> `ThreadFunc`）。
   - 调用 `Thread::Start()` 启动线程。
   - 等待 `m_loop` 被设置。
   - 返回后调用者获得有效 `EventLoop*`。
3. **`ThreadFunc()`**：
   - 在栈上构造 `EventLoop`。
   - 加锁设置 `m_loop`，调用 `m_initCallback(&loop)`。
   - 通知 `Start()` 继续。
   - 调用 `loop.Loop()`。
   - `Loop()` 返回后，加锁清空 `m_loop`。
4. **`Stop()` / 析构**：
   - 标记 `m_stopped`。
   - 通过 `m_loop->RunInLoop([loop]{ loop->Quit(); })` 唤醒并退出循环。
   - `m_thread.reset()` 触发 `Thread` 析构并 Join。

### 启动时序

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
   |   m_loop available           |<------------------------|
   |<---------------------------- |                         |
   |   loop.Loop()                |                         |
```

### 停止时序

```
Main Thread
   | Stop()
   |   m_stopped = true
   |   m_loop->RunInLoop(Quit)
   |   m_thread.reset()  // 析构 Thread -> Join
   |<-- 线程退出，EventLoop 栈对象销毁
```

## 5. 设计要点

- **非移动**：线程函数捕获 `this`，移动后 `this` 失效，因此显式删除移动构造和赋值。
- **不拥有 EventLoop**：`EventLoop` 是线程栈对象，`EventLoopThread` 只持有指针，保证生命周期清晰。
- **复用 Thread**：通过 `std::unique_ptr<Thread>` 管理，避免与 `Thread` 的 RAII 行为冲突。
- **异常安全**：`InitCallback` 中捕获异常并记录日志，避免未处理异常导致 `std::terminate`。
- **自停保护**：如果 `Stop()` 被从 loop 线程自身调用，不会在该线程中 Join 自己，避免死锁。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| 未 Start 就析构 | `Stop()` 检查 `m_started`，直接返回。 |
| 多次 Start | 第二次直接返回，不创建新线程。 |
| 多次 Stop | 第二次直接返回，避免重复 Join。 |
| Stop() 时 loop 尚未就绪 | `m_started` 保证不会启动。 |
| 线程名空 | 底层 `Thread` 使用默认名。 |

## 7. 测试覆盖

- `StartReturnsLoop`：启动后返回非空 `EventLoop*`，且 `IsInLoopThread()` 为真。
- `InitCallbackRunsInLoopThread`：初始化回调在 loop 线程中执行，并拿到正确指针。
- `ThreadNameIsSet`：通过 `InitCallback` 验证线程名被正确设置。
- `RunInLoopFromOtherThread`：从其他线程提交任务，loop 线程中执行。
- `MultipleStartReturnsSameLoop`：重复 `Start()` 返回同一指针。
- `StopIsIdempotent`：多次 `Stop()` 不崩溃。
- `LoopCanQuitItself`：loop 线程内定时器触发 `Quit()` 后，线程能正常退出。
- `DestructorStopsRunningThread`：析构自动停止并 Join 运行中的线程。

## 8. 示例

```cpp
#include "solar_net/event_loop_thread.h"

#include <atomic>
#include <chrono>
#include <format>
#include <iostream>
#include <thread>

int main() {
    solar_net::EventLoopThread loopThread("example");
    loopThread.Start();

    solar_net::EventLoop* loop = loopThread.GetLoop();
    std::atomic<int> counter{0};

    loop->RunEvery(std::chrono::milliseconds(500), [&] {
        std::cout << std::format("tick {}\n",
                                 counter.fetch_add(1, std::memory_order_relaxed));
    });

    std::this_thread::sleep_for(std::chrono::seconds(2));
    loopThread.Stop();

    return 0;
}
```

## 9. 性能

- 启动一次：一次线程创建 + 一次 `mutex/cond` 唤醒，属于低频操作。
- 运行时无共享锁：主线程通过 `EventLoop*` 调用 `RunInLoop`，无需 `EventLoopThread` 锁。
- 停止时：一次跨线程 `eventfd` 唤醒 + 线程 Join。

Benchmark 为 `Start/Stop` 周期，用于观察线程创建和 EventLoop 初始化的开销。

## 10. 下一步

实现 `EventLoopThreadPool`，它持有多个 `EventLoopThread` 实例，按 Round-Robin 或其他策略分配 `EventLoop*`，为后续 TCP Acceptor 与多线程 IO 做准备。
