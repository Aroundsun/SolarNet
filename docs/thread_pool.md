# ThreadPool

`ThreadPool` 是固定大小的任务线程池，带阻塞任务队列，适用于 CPU 密集型或阻塞型后台任务。与 `EventLoopThreadPool`（IO Reactor）职责不同，二者互补。

## 1. 职责

- 维护 N 个工作线程与 FIFO 任务队列。
- 提供 `Submit` 投递 `std::function<void()>` 任务。
- `Stop` 通知 worker 在队列排空后退出；`Wait` 阻塞至全部结束。
- 捕获任务异常并记录日志，避免 worker 因未捕获异常终止。

## 2. 类图与生命周期

```
+----------------------------------+
| ThreadPool : NonCopyable         |
+----------------------------------+
| - m_thread_count: size_t         |
| - m_name: string                 |
| - m_threads: vector<Thread>      |
| - m_tasks: queue<Task>           |
| - m_mutex / m_cv                 |
| - m_started / m_stopping / m_stopped |
+----------------------------------+
| + Start()                        |
| + Stop()                         |
| + Wait()                         |
| + Submit(task) -> bool           |
| - RunWorker(index)               |
+----------------------------------+
              | owns N
              v
+----------------------------------+
| Thread                           |
+----------------------------------+
```

生命周期：

1. **构造**：保存线程数与名称前缀，不创建线程。
2. **`Start()`**：创建 N 个 `Thread`，每个执行 `RunWorker`。
3. **`Submit()`**：入队并 notify worker。
4. **`Stop()`**：置 `m_stopping`，worker 处理完队列后退出。
5. **`Wait()`**：等待所有 `Thread` Join 完成。
6. **析构**：若仍在运行，自动 `Stop()` + `Wait()`。

## 3. API

```cpp
namespace solar_net {

class ThreadPool : NonCopyable {
 public:
  using Task = std::function<void()>;

  explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency(),
                      std::string name = {});
  ~ThreadPool();

  void Start();
  void Stop();
  void Wait();

  bool Submit(Task task);

  size_t ThreadCount() const noexcept;
  size_t PendingTaskCount() const noexcept;
  bool IsRunning() const noexcept;
};

}  // namespace solar_net
```

头文件：`#include "solar_net/base/thread_pool.h"`

## 4. 关键流程

### 任务调度

```
Submit(task)
   | lock(m_mutex)
   | if stopping -> return false
   | m_tasks.push(task)
   | notify_one(m_cv)

Worker
   | wait until task || stopping
   | pop task -> run (try/catch -> LOG_ERROR)
   | if stopping && queue empty -> exit
```

### 关闭流程

```
Stop()
   | m_stopping = true
   | notify_all

Wait()
   | for each thread: Join()
   | m_stopped = true
```

## 5. 设计要点

- **与 EventLoopThreadPool 区分**：ThreadPool 跑通用任务；EventLoopThreadPool 跑 IO 事件循环。
- **thread_count = 0**：按 `hardware_concurrency`，失败则为 1。
- **仅 Wait 不 Stop**：会永久阻塞（文档/架构中已标注）。
- **异常隔离**：单任务异常不影响其他 worker。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| Stop 后 Submit | 返回 false。 |
| 仅 Wait 不 Stop | 永久阻塞。 |
| 多次 Start | 第二次 no-op。 |
| 任务抛异常 | 捕获并 LOG_ERROR，worker 继续。 |
| 析构时仍在运行 | 自动 Stop + Wait。 |

## 7. 测试覆盖

- `RunsTasks`：多任务被执行。
- `SubmitFailsAfterStop`：Stop 后 Submit 失败。
- `PendingTaskCountTracksTasks`：队列长度正确。
- `DestructorAutoStopsAndWaits`：析构自动清理。
- `TaskExceptionDoesNotCrashPool`：异常不崩溃。
- `ReportsCorrectThreadCount`：线程数正确。

## 8. 示例

```cpp
#include "solar_net/base/thread_pool.h"

#include <atomic>

int main() {
    solar_net::ThreadPool pool(4, "demo");
    pool.Start();

    std::atomic<int> counter{0};
    for (int i = 0; i < 20; ++i) {
        pool.Submit([&] { counter.fetch_add(1); });
    }

    pool.Stop();
    pool.Wait();
    return 0;
}
```

运行：

```bash
./build/examples/example_thread_pool
```

## 9. 性能

- Submit：O(1) 入队 + 一次 notify。
- 适合粗粒度任务；细粒度高频任务应考虑专用线程或 EventLoop。

Benchmark：`bench_thread_pool`，测量 Submit/执行吞吐。

## 10. 下一步

- 支持 `future`/`promise` 返回值。
- 优雅关闭超时。
- 与 RPC/任务调度模块集成；IO 路径仍用 [EventLoopThreadPool](event_loop_thread_pool.md)。
