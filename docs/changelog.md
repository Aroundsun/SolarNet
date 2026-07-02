# SolarNet 开发日志

按时间倒序记录主要里程碑。详细设计见各模块文档与 [architecture.md](architecture.md)。

---

## 2026-07-02 — Phase 4 传输层（InetAddress / Socket / Acceptor）

### 新增模块

- **InetAddress**：IPv4/IPv6 地址值类型
- **Socket**：socket fd RAII（bind/listen/accept/connect）
- **Acceptor**：监听 socket + Channel，回调交出 conn_fd

### 测试与基准

- 单测增至 **112** 项（+29）
- 新增 example/benchmark 各 3 个

### 修复

- 统一 include 路径为 `solar_net/net/transport/`、`solar_net/base/`
- `inet_pton` 使用 null -terminated 字符串
- Acceptor 须在 loop 线程创建/销毁；析构仅在 loop 线程 Remove Channel

---

## 2026-07-02 — Phase 3 网络核心完成

### 新增模块

- **Channel**：fd 事件通道，读/写/关闭/错误回调 + Tie 生命周期绑定
- **Poller / EpollPoller**：IO 多路复用抽象与 Linux epoll 实现
- **EventLoop**：Reactor 主循环，eventfd 唤醒，pending functors
- **TimerQueue**：timerfd 定时器，RunAt/RunAfter/RunEvery/Cancel
- **EventLoopThread**：后台线程运行 EventLoop，Start 返回 `EventLoop*`
- **EventLoopThreadPool**：多 Reactor 线程池，Round-Robin 分配

### 测试与基准

- 单测由 38 增至 **83** 项，全部通过
- 新增 benchmark：channel、poller、event_loop、timer_queue、event_loop_thread、event_loop_thread_pool

### 文档

- 14 个模块设计文档（统一 10 节结构）
- [docs/README.md](README.md) 模块索引
- [review.md](review.md) Phase 1–3 评审报告
- [examples.md](examples.md) 10 个示例说明

### 修复

- EventLoop Quit 后仍处理 pending functors
- TimerQueue 无定时器时 disarm timerfd
- EventLoopThread 跨线程定时器须 RunInLoop 注册（测试/示例修正）

---

## 2026-07-02 — Phase 2 并发原语

### 新增

- **Thread**：std::thread RAII，命名，析构自动 Join
- **ThreadPool**：固定大小任务池，Submit/Stop/Wait

### 修复

- `Thread::Join()` / `Detach()` CAS 逻辑错误

### 测试

- test_thread（6）、test_thread_pool（6）
- bench_thread_pool

---

## 2026-07-02 — Phase 1 基础模块

### 新增

- **Time**：时间戳、格式化、`<=>` 比较
- **Logger**：分级日志、Formatter/Appender、LOG_* 宏
- **Buffer**：muduo 三区布局，readv/writev，CRLF 查找

### 修复

- `Logger::SetLevel` 改为 `std::atomic<LogLevel>`
- test_time 正则 `\d` → `[0-9]`

### 测试

- test_version、test_time、test_logger、test_buffer
- bench_buffer、bench_version

---

## 2026-07-02 — Phase 0 工程基础设施

- CMake 3.20+、C++20、GoogleTest / Benchmark 系统包依赖
- `.clang-format`、`.clang-tidy`、GitHub Actions CI
- 目录结构：`solar_net/`、`tests/`、`benchmarks/`、`examples/`、`docs/`
