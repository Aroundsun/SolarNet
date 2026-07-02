# SolarNet 模块文档

各模块设计文档，结构统一为：职责 → 类图与生命周期 → API → 关键流程 → 设计要点 → 边界情况 → 测试覆盖 → 示例 → 性能 → 下一步。

## 基础层（`solar_net/base/`）

| 模块 | 文档 | 说明 |
| --- | --- | --- |
| Version | [version.md](version.md) | 项目名与版本号 |
| Time | [time.md](time.md) | 时间戳与格式化 |
| Logger | [logger.md](logger.md) | 分级日志与 Appender |
| Buffer | [buffer.md](buffer.md) | 网络 I/O 缓冲区 |
| Thread | [thread.md](thread.md) | std::thread RAII 封装 |
| ThreadPool | [thread_pool.md](thread_pool.md) | 固定大小任务线程池 |

## 网络层（`solar_net/net/`）

| 模块 | 文档 | 说明 |
| --- | --- | --- |
| Channel | [channel.md](channel.md) | fd 事件通道与回调分发 |
| Poller / EpollPoller | [poller.md](poller.md) | IO 多路复用（epoll） |
| EventLoop | [event_loop.md](event_loop.md) | Reactor 事件循环 |
| TimerQueue | [timer_queue.md](timer_queue.md) | timerfd 定时器 |
| EventLoopThread | [event_loop_thread.md](event_loop_thread.md) | 单线程 EventLoop 封装 |
| EventLoopThreadPool | [event_loop_thread_pool.md](event_loop_thread_pool.md) | 多 Reactor 线程池 |

## 传输层（`solar_net/net/transport/`）

| 模块 | 文档 | 说明 |
| --- | --- | --- |
| InetAddress | [inet_address.md](inet_address.md) | IPv4/IPv6 地址 |
| Socket | [socket.md](socket.md) | socket fd RAII |
| Acceptor | [acceptor.md](acceptor.md) | TCP 连接接受 |

## 其他文档

| 文档 | 说明 |
| --- | --- |
| [architecture.md](architecture.md) | 整体架构与阶段规划 |
| [review.md](review.md) | 设计/代码评审、测试与 Benchmark 报告 |
| [changelog.md](changelog.md) | 开发日志 |
| [examples.md](examples.md) | 示例程序说明 |

## 推荐阅读顺序

1. [architecture.md](architecture.md) — 全局视图  
2. 基础层：Time → Logger → Buffer → Thread → ThreadPool  
3. 网络层：Channel → Poller → EventLoop → TimerQueue → EventLoopThread → EventLoopThreadPool  
