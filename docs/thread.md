# Thread

`Thread` 是对 `std::thread` 的 RAII 封装，支持命名、单次启动，以及析构时自动 Join。被 `ThreadPool`、`EventLoopThread` 等模块复用。

## 1. 职责

- 包装 `std::function<void()>` 为后台线程任务。
- 启动前设置 Linux 线程名（最多 15 字节）。
- 保证每个实例最多 Start 一次。
- 析构时若已启动且未 Join/Detach，自动 Join。

## 2. 类图与生命周期

```
+---------------------------+
| Thread : NonCopyable      |
+---------------------------+
| - m_func: function        |
| - m_name: string          |
| - m_thread: std::thread   |
| - m_started: atomic<bool>   |
| - m_joined: atomic<bool>    |
| - m_detached: atomic<bool>  |
+---------------------------+
| + Start()                 |
| + Join()                  |
| + Detach()                |
| + IsStarted()             |
| + SetCurrentThreadName()  |
| + GetCurrentThreadName()  |
+---------------------------+
```

生命周期：

1. **构造**：保存回调与线程名，不创建 OS 线程。
2. **`Start()`**：创建 `std::thread`，在线程入口设置线程名后执行回调。
3. **`Join()` / `Detach()`**：等待或分离；CAS 保证只执行一次。
4. **析构**：若仍运行且未 detach，自动 Join。

## 3. API

```cpp
namespace solar_net {

class Thread : NonCopyable {
 public:
  using ThreadFunc = std::function<void()>;

  explicit Thread(ThreadFunc func, std::string name = {});
  ~Thread();

  void Start();
  void Join();
  void Detach();

  bool IsStarted() const noexcept;
  std::thread::id GetId() const noexcept;
  const std::string& GetName() const noexcept;

  static void SetCurrentThreadName(std::string_view name);
  static std::string GetCurrentThreadName();
};

}  // namespace solar_net
```

头文件：`#include "solar_net/base/thread.h"`

## 4. 关键流程

### 启动流程

```
Start()
   | assert !m_started
   | m_thread = std::thread([this] {
   |     SetCurrentThreadName(m_name);
   |     m_func();
   | })
   | m_started = true
```

### 析构自动 Join

```
~Thread()
   | if started && !joined && !detached
   |     Join()
```

## 5. 设计要点

- **NonCopyable**：线程资源不可拷贝，避免双重 Join。
- **Join 幂等**：`m_joined` CAS 保证重复 Join 安全。
- **命名**：便于 `top` / `perf` / 日志中识别 IO 线程与 worker 线程。
- **与 ThreadPool 分工**：Thread 管单线程生命周期，ThreadPool 管队列与调度。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| 未 Start 就 Join | 无操作（未启动）。 |
| Start 后再次 Start | 未定义/依赖 assert（不应重复调用）。 |
| Join 后再 Detach | Detach 被 CAS 拒绝。 |
| 线程名超过 15 字节 | Linux 内核截断。 |
| 空线程名 | 使用系统默认名。 |

## 7. 测试覆盖

- `RunsCallback`：回调被执行。
- `IsStartedAfterStart`：Start 后标志为真。
- `GetIdReturnsValidId`：有效 thread id。
- `DestructorAutoJoins`：析构等待线程结束。
- `NameIsAccessible`：构造名可读。
- `CurrentThreadNameRoundTrip`：Set/Get 线程名一致。

## 8. 示例

`Thread` 通常不单独写示例，而是通过 [ThreadPool](thread_pool.md) 或 [EventLoopThread](event_loop_thread.md) 使用：

```cpp
#include "solar_net/base/thread.h"

solar_net::Thread t([] {
    solar_net::Thread::SetCurrentThreadName("worker");
    // ...
}, "worker");
t.Start();
t.Join();
```

## 9. 性能

- Start：一次线程创建，适合低频（IO 线程、池 worker）。
- 运行时无 Thread 对象开销，回调在独立线程执行。

无独立 Benchmark。

## 10. 下一步

- [ThreadPool](thread_pool.md) 持有多个 `Thread` 执行计算任务。
- [EventLoopThread](event_loop_thread.md) 用 `Thread` 驱动 `EventLoop::Loop()`。
