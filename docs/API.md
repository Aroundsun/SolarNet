# SolarNet API 文档

本文档面向组内开发者，说明 SolarNet 的公开 API、线程模型、生命周期约定与推荐用法。阅读前建议先跑通 `example/echo_server.cpp`。

---

## 目录

1. [概述](#1-概述)
2. [集成与编译](#2-集成与编译)
3. [线程模型与调用约束](#3-线程模型与调用约束)
4. [推荐生命周期](#4-推荐生命周期)
5. [模块 API 参考](#5-模块-api-参考)
   - [Buffer](#51-buffer)
   - [Socket](#52-socket)
   - [EventLoop](#53-eventloop)
   - [定时器](#54-定时器)
   - [EventLoopThread](#55-eventloopthread)
   - [EventLoopThreadPool](#56-eventloopthreadpool)
   - [TcpConnection](#57-tcpconnection)
   - [TcpServer](#58-tcpserver)
6. [回调语义](#6-回调语义)
7. [使用规范与反模式](#7-使用规范与反模式)
8. [常见问题](#8-常见问题)

---

## 1. 概述

SolarNet 是 Linux 下的 C++ TCP 网络库，采用 **Reactor** 模型：

- 每个线程最多一个 `EventLoop`，由 `epoll` 驱动 I/O 多路复用。
- 文件描述符通过 `Channel` 注册到 `EventLoop`，事件到达时触发回调。
- `TcpServer` 将 `Acceptor`、连接管理、`EventLoopThreadPool` 组装为完整服务端。

命名空间：`solar_net`。公开头文件均在 `src/` 目录，编译时将该目录加入 include 路径即可。

```
主线程 EventLoop
  ├── Acceptor：监听、accept 新连接
  └── EventLoopThreadPool（可选）
        ├── IO 线程 1 → EventLoop
        ├── IO 线程 2 → EventLoop
        └── ...
```

| 组件 | 头文件 | 业务代码是否直接使用 |
|------|--------|----------------------|
| `Buffer` | `buffer.h` | 是（在消息回调中处理数据） |
| `Socket` | `socket.h` | 偶尔（静态工具方法） |
| `EventLoop` | `event_loop.h` | 是 |
| `TcpConnection` | `tcp_connection.h` | 是（通过回调获得） |
| `TcpServer` | `tcp_server.h` | 是 |
| `EventLoopThread` / `EventLoopThreadPool` | `event_loop_thread.h` / `event_loop_thread_pool.h` | 一般通过 `TcpServer` 间接使用 |
| `Acceptor` | `acceptor.h` | 一般通过 `TcpServer` 间接使用 |
| `Channel` / `EpollPoller` / `TimerQueue` | 各自头文件 | **否**（库内部实现，须遵守线程约束） |

---

## 2. 集成与编译

### 2.1 作为子项目链接

```cmake
add_subdirectory(path/to/SolarNet)
target_link_libraries(your_target PRIVATE solar_net)
# solar_net 已 PUBLIC 暴露 ${SolarNet_SOURCE_DIR}/src 为 include 路径
```

### 2.2 直接引用静态库

```cmake
target_include_directories(your_target PRIVATE /path/to/SolarNet/src)
target_link_libraries(your_target PRIVATE /path/to/SolarNet/build/lib/libsolar_net.a)
```

### 2.3 依赖

- Linux（`epoll`、`eventfd`）
- C++17
- 无第三方运行时依赖

### 2.4 最小服务端骨架

```cpp
#include <arpa/inet.h>
#include "event_loop.h"
#include "tcp_connection.h"
#include "tcp_server.h"

int main() {
    solar_net::EventLoop loop;

    ::sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    solar_net::TcpServer server(&loop, addr, "my-server");
    server.set_thread_num(0);  // 必须在 start() 之前

    server.set_message_callback([](const solar_net::TcpConnectionPtr& conn,
                                   solar_net::Buffer* buf,
                                   int64_t) {
        conn->send(buf);
    });

    server.start();
    loop.loop();       // 阻塞
    server.stop();     // loop 退出后、同一线程显式关停
    return 0;
}
```

---

## 3. 线程模型与调用约束

这是使用本库**最重要**的一节。违反约束会导致 `abort()`、死锁或资源泄漏。

### 3.1 基本原则

| 规则 | 说明 |
|------|------|
| 一线程一 Loop | 每个线程最多创建一个 `EventLoop`；同线程创建第二个会 `abort()` |
| Loop 线程专属 | `Channel`、`TcpConnection` 内部操作、`Acceptor::listen/stop_listening` 必须在所属 `EventLoop` 线程执行 |
| 跨线程投递 | 通过 `EventLoop::run_in_loop` / `queue_in_loop` 将任务投递到目标 loop 线程 |
| 唤醒机制 | 跨线程投递时写 `eventfd` 唤醒阻塞在 `epoll_wait` 上的 loop |

### 3.2 API 线程安全一览

| API | 线程安全 | 说明 |
|-----|----------|------|
| `EventLoop::stop()` | ✅ | 任意线程可调用 |
| `EventLoop::run_in_loop()` | ✅ | 非 loop 线程投递到队列 |
| `EventLoop::queue_in_loop()` | ✅ | 总是入队，即使已在 loop 线程 |
| `EventLoop::run_after()` / `run_every()` / `cancel()` | ✅ | 内部投递到 loop 线程 |
| `TcpConnection::send()` | ✅ | 内部 `run_in_loop` |
| `TcpConnection::shutdown()` / `force_close()` | ✅ | 内部 `run_in_loop` |
| `TcpServer::stop()` | ⚠️ 有条件 | 见 [3.3](#33-tcpserverstop-调用约束) |
| `TcpServer::set_*_callback()` | ❌ | 无同步，须在 `start()` 前于固定线程设置 |
| `TcpConnection::set_*_callback()` | ❌ | 须在所属 IO 线程、连接活跃前设置 |
| `Buffer` 全部方法 | ❌ | 在所属连接的 IO 线程使用 |
| `Channel` 全部方法 | ❌ | 在所属 loop 线程使用 |
| 回调函数体 | — | 在对应 loop 线程执行，**禁止阻塞** |

### 3.3 TcpServer::stop() 调用约束

`stop()` 与 `~TcpServer()`（析构内部调 `stop()`）遵守相同规则：

| 时机 | 调用线程 |
|------|----------|
| `loop.loop()` **仍在运行** | 任意线程（通过 `run_in_loop` 投递到主 loop） |
| `loop.loop()` **已返回** | **必须在主 loop 所属线程** |

`loop.loop()` 返回后主 loop 已停止。此时若从其他线程调 `stop()`，仍会走 `run_in_loop`，但任务无法被派发，连接与 IO 线程池可能无法正确清理。

**正确模式**（`example/echo_server.cpp`）：

```cpp
// 信号处理：只停止 loop，不直接 stop server
void on_signal(int) { g_loop->stop(); }

loop.loop();      // 返回后 loop 已停
server.stop();    // 在 loop 所属线程（main）显式关停
// server 析构；若已 stop()，析构中的 stop() 因 started_=false 直接返回
```

### 3.4 stop() 后不可重启

`stop()` 会 `thread_pool_.reset()`，释放 IO 线程池，**没有**在 `stop()` 后再次 `start()` 重建线程池的路径。`TcpServer` 为一次性关停（与 muduo 类似）：`start()` → `stop()` 后应析构对象，需要重启服务请重新构造 `TcpServer`。

```cpp
server.start();
loop.loop();
server.stop();
// server.start();  // ❌ 无效：线程池已释放，无法恢复
```

**错误模式**：

```cpp
std::thread t([&]() {
    loop.loop();
});
t.join();
server.stop();  // ❌ 若 stop 不在 loop 所属线程，关停任务可能永不执行
```

### 3.4 run_in_loop 与 queue_in_loop

```cpp
void run_in_loop(Task task);   // 已在 loop 线程 → 立即执行；否则入队
void queue_in_loop(Task task); // 总是入队，本轮 poll 结束后执行
```

- 一般业务用 `run_in_loop` 即可。
- 若**已在 loop 线程**且希望任务排到当前回调之后（避免重入），用 `queue_in_loop`。

### 3.5 IO 线程分配

```cpp
server.set_thread_num(N);  // 必须在 start() 之前
```

| `N` | 行为 |
|-----|------|
| `0` | 单线程：accept 与所有连接读写均在主 loop |
| `> 0` | 主 loop 只 accept；新连接 round-robin 分配到 N 个 IO 线程的 loop |

每个连接的回调（`connection` / `message` / `write_complete`）在**该连接所属 IO 线程**执行，不在主线程。

### 3.6 回调与对象成员的线程安全

库内**仅部分 API 做了跨线程投递**（如 `TcpConnection::send`、`EventLoop::stop`、定时器）。回调成员本身是普通 `std::function` 字段，**读写无锁、无原子保护**。

#### 线程安全的操作

| 操作 | 机制 |
|------|------|
| `TcpConnection::send` / `shutdown` / `force_close` | 内部 `run_in_loop` 到所属 IO 线程 |
| `EventLoop::stop` / `run_in_loop` / `queue_in_loop` | 任务队列 + `eventfd` 唤醒 |
| `EventLoop::run_after` / `run_every` / `cancel` | 投递到 loop 线程 |

#### 无保护的回调设置

`TcpServer` 与 `TcpConnection` 的 `set_connection_callback`、`set_message_callback`、`set_write_complete_callback`、`set_high_water_mark_callback`、`set_close_callback`（及 `TcpServer::set_thread_init_callback`）均为**直接赋值**，跨线程调用或与 IO 线程并发读写同一字段构成**未定义行为（数据竞争）**。

`new_connection` 时会把 `TcpServer` 上的回调**拷贝**到各 `TcpConnection`。因此：

- **推荐**：在 `server.start()` **之前**、于主线程一次性设置 `TcpServer` 全部回调，运行期不再修改。
- 运行中跨线程 `set_message_callback` 等：不仅与 IO 线程读回调竞态，且**已建立连接仍持有旧回调**，新连接才拿到新回调。
- 若需按连接定制逻辑，在 `connection_callback` 内分支，或将可变状态放在外部并用 `shared_ptr` / 原子指针指向，而非替换回调本身。

`TcpConnection::set_*_callback` 须在**该连接所属 IO 线程**、且最好在 `connection_established` 之前调用（`TcpServer` 路径下由库在 `new_connection` 中设置，业务一般无需直接调）。

#### TcpServer::stop 与 close_callback

`stop_in_loop` 在强制关闭前将各连接的 `close_callback` 置空，避免 `handle_close` 再次触发 `remove_connection`（此时 `connections_` 已清空）。清空操作在**各连接的 IO 线程**内、于 `force_close` 之前执行，与 `handle_close` 读 `close_cb_` 串行。

仍存在的理论窗口（概率低）：`stop()` 与对端自然断开并发时，IO 线程可能**已在执行** `handle_close` 并调用旧的 `close_callback`。通常无害（`remove_connection_in_loop` 对不在 map 中的连接是空操作），但严格意义上 shutdown 路径并非形式化无竞态。

---

## 4. 推荐生命周期

### 4.1 服务端标准流程

```
构造 EventLoop
    ↓
构造 TcpServer(&loop, addr, name)
    ↓
set_thread_num / 设置各类回调
    ↓
server.start()          // 启动 IO 线程池 + 开始 listen
    ↓
loop.loop()             // 阻塞，处理事件
    ↓
server.stop()           // loop 退出后、同线程关停
    ↓
server / loop 析构
```

### 4.2 优雅退出（信号）

```cpp
solar_net::EventLoop* g_loop = nullptr;

void on_signal(int) {
    if (g_loop) g_loop->stop();  // 仅 stop loop
}

int main() {
    solar_net::EventLoop loop;
    g_loop = &loop;
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    // ... 构造 server、start ...
    loop.loop();
    server.stop();
    g_loop = nullptr;
}
```

信号处理函数中**不要**调用 `TcpServer::stop()` 或执行复杂逻辑；只调 `EventLoop::stop()` 让 `loop.loop()` 返回，再在 main 线程完成清理。

### 4.3 start() 之前 vs 之后

| 操作 | 时机 |
|------|------|
| `set_thread_num()` | `start()` **之前** |
| `set_*_callback()` | `start()` 之前或之后均可（建议之前） |
| `start()` | 可重复调用逻辑安全，但仅首次生效 |
| `stop()` | `start()` 之后；`loop.loop()` 返回后在同线程调用。**一次性关停，不可 stop 后再 start()** |

---

## 5. 模块 API 参考

### 5.1 Buffer

网络读写缓冲区，底层 `std::vector<uint8_t>`，读写下标分离，头部预留 8 字节用于 cheap prepend（协议编解码场景）。

#### 构造

```cpp
explicit Buffer(std::size_t initial_size = 1024);
```

#### 读侧

| 方法 | 说明 |
|------|------|
| `readable_bytes()` | 可读字节数 |
| `data()` | 指向可读区域首地址 |
| `retrieve(n)` | 消费 n 字节；n 超过可读量则 `retrieve_all()` |
| `retrieve_all()` | 重置读写索引，不释放容量 |
| `retrieve_as_string(n)` | 读取 n 字节为 `std::string` 并消费 |
| `retrieve_all_as_string()` | 读取全部可读数据并消费 |
| `peek_int32()` | 预览 int32（主机字节序，不消费） |
| `read_int32()` | 读取并消费 int32 |

#### 写侧

| 方法 | 说明 |
|------|------|
| `writable_bytes()` | 可写空间（含未扩容部分） |
| `ensure_writable_bytes(len)` | 保证至少 len 字节可写 |
| `append(data, len)` / `append(string)` / `append(Buffer)` | 追加数据 |
| `has_written(len)` | 手动前移写索引（配合外部写入） |
| `unwrite(len)` | 回退写索引 |

#### 前置写入

| 方法 | 说明 |
|------|------|
| `prependable_bytes()` | 读索引前的可用空间 |
| `prepend(data, len)` | 在读索引前写入（用于加包头） |
| `prepend_int32(val)` | 前置 int32 |

#### 其他

| 方法 | 说明 |
|------|------|
| `read_from_fd(fd)` | `readv` 读 socket；返回 `>0` 字节数，`0` EOF，`-1` 错误（含 `EAGAIN`/`EWOULDBLOCK`，errno 保留） |
| `shrink()` | 收缩内部 vector |
| `swap(other)` | 交换 |

#### 在消息回调中使用

```cpp
server.set_message_callback([](const TcpConnectionPtr& conn, Buffer* buf, int64_t) {
    // 方式 1：取走全部数据
    std::string msg = buf->retrieve_all_as_string();

    // 方式 2：按协议解析，只消费已处理部分
    while (buf->readable_bytes() >= 4) {
        uint32_t len = /* 从 buf->data() 解析 */;
        if (buf->readable_bytes() < 4 + len) break;
        buf->retrieve(4);
        std::string body = buf->retrieve_as_string(len);
        // 处理 body...
    }

    conn->send("ok");
});
```

**注意**：未 `retrieve` 的数据会留在 `input_buffer` 中，下次读事件会继续累加。半包粘包需自行处理。

---

### 5.2 Socket

RAII 封装 socket fd，提供静态工具方法。

```cpp
explicit Socket(int fd);           // 析构时 close(fd)
int fd() const;

static int set_non_blocking(int fd);
static int set_keep_alive(int fd);
static int set_tcp_no_delay(int fd);
static int set_reuse_addr(int fd);
static int set_reuse_port(int fd);
static ::sockaddr_in get_local_addr(int fd);
static ::sockaddr_in get_peer_addr(int fd);
```

不可拷贝，可移动。`TcpConnection` 内部持有 `Socket`，业务一般无需直接创建。

---

### 5.3 EventLoop

核心事件循环，**每个线程一个**。

#### 构造与析构

```cpp
EventLoop();
~EventLoop();
```

同线程重复构造会 `abort()`。析构前须已退出 `loop()`。

#### 运行控制

```cpp
void loop();   // 阻塞运行，须在 loop 所属线程调用
void stop();   // 线程安全，请求退出 loop()
```

`loop()` 每次调用前会清除启动前积累的 wakeup 计数；若 `stop()` 在 `loop()` 开始前已被调用，则 `loop()` 立即返回。

#### 任务调度

```cpp
using Task = std::function<void()>;

void run_in_loop(Task task);
void queue_in_loop(Task task);
void assert_in_loop_thread();   // 违反则 abort
bool is_in_loop_thread() const;
```

#### Channel 管理（库内部使用，了解即可）

```cpp
void update_channel(Channel* channel);
void remove_channel(Channel* channel);
```

#### 线程本地查询

```cpp
static EventLoop* get_event_loop_of_current_thread();
```

在 IO 线程的 `thread_init_callback` 中可拿到该线程的 `EventLoop*`。

---

### 5.4 定时器

通过 `EventLoop` 注册，基于 `timerfd`，**可跨线程调用**。

```cpp
using TimerCallback = std::function<void()>;

TimerId run_after(double delay_seconds, TimerCallback cb);   // 一次性
TimerId run_every(double interval_seconds, TimerCallback cb); // 重复
void cancel(TimerId timer_id);
```

```cpp
// 示例
auto id = loop.run_after(0.5, [] { /* 0.5 秒后执行 */ });
auto id2 = loop.run_every(1.0, [] { /* 每秒执行 */ });
loop.cancel(id);
```

- `delay` / `interval` 单位为**秒**（`double`）。
- 回调在所属 `EventLoop` 线程执行。
- `TimerId` 默认构造表示无效 ID；`cancel` 对无效 ID 安全。
- `poll` 超时与最近定时器联动，`kPollTimeoutMs = 10000` 为兜底上限。

#### cancel 语义与实现

`add_timer` / `cancel` 均通过 `run_in_loop` 投递到 loop 线程执行，对外**线程安全**。`add_timer` 在调用线程先 `new Timer`，再投递 `add_timer_in_loop` 入队；`TimerId` 立即返回。

**竞态窗口（已修复）**：若 `cancel_in_loop` 先于 `add_timer_in_loop` 执行，且不在过期回调期间，定时器尚未进入 `timers_`，旧实现会直接返回，导致 cancel 无效、回调仍会触发。muduo 存在同类窗口。

`TimerQueue` 用两个集合分别处理不同阶段的 cancel：

| 集合 | 用途 |
|------|------|
| `pending_cancel_timers_` | 定时器**尚未入队**即被 cancel（add/cancel 投递顺序竞态） |
| `canceling_timers_` | 定时器**正在执行过期回调**期间被 cancel |

处理逻辑：

1. **`cancel_in_loop`**：在 `timers_` 中找到则 erase；未找到时，若 `calling_expired_timers_` 为真则加入 `canceling_timers_`，否则加入 `pending_cancel_timers_`。
2. **`add_timer_in_loop`**：入队前检查 `pending_cancel_timers_`；若已标记取消则 delete 并返回，不再 insert。
3. **`handle_read`**：执行过期回调时跳过 `canceling_timers_` 中的定时器；回调结束后统一清理。

`pending_cancel_timers_` 不可与 `canceling_timers_` 合并：`handle_read` 结束时会 delete `canceling_timers_` 中的指针；若把「待入队即取消」的定时器也放进去，可能在 `add_timer_in_loop` 运行前被提前释放，造成 use-after-free。

典型跨线程用法（add 后立即 cancel）：

```cpp
// 工作线程
TimerId id = loop.run_after(1.0, callback);
loop.cancel(id);  // 安全，即使 cancel 先于 add 入队
```

对应测试：`test_timer` 中的 `CancelBeforeAddInLoop`。

---

### 5.5 EventLoopThread

在独立线程中运行一个 `EventLoop`。

```cpp
using ThreadInitCallback = std::function<void(EventLoop*)>;

explicit EventLoopThread(const ThreadInitCallback& cb = {});
~EventLoopThread();

EventLoop* start_loop();  // 启动线程，阻塞直到 loop 初始化完成
```

析构时会对 loop 调 `stop()` 并 `join` 线程。一般由 `EventLoopThreadPool` 管理，业务少直接使用。

---

### 5.6 EventLoopThreadPool

IO 线程池，由 `TcpServer` 内部持有。

```cpp
explicit EventLoopThreadPool(EventLoop* base_loop);

void set_thread_num(int num_threads);  // start() 前
void start(const ThreadInitCallback& cb = {});

EventLoop* get_next_loop();              // round-robin
EventLoop* get_loop(std::size_t index) const;
const std::vector<EventLoop*>& get_all_loops() const;
EventLoop* base_loop() const;
bool started() const;
std::size_t thread_num() const;
```

`num_threads == 0` 时 `get_next_loop()` 始终返回 `base_loop`（主 loop）。

---

### 5.7 TcpConnection

表示一条已建立的 TCP 连接，由 `shared_ptr` 管理。

```cpp
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
```

#### 连接状态

```cpp
enum class State {
    kConnecting,     // 已创建，尚未 connection_established
    kConnected,      // 已连接
    kDisconnecting,  // 正在关闭写端
    kDisconnected    // 已断开
};
```

#### 构造（由 TcpServer 内部创建，了解命名规则即可）

连接名格式：`{server_name}-{id}#{peer_ip}:{peer_port}`

#### 只读访问

```cpp
EventLoop* get_loop() const;
const std::string& name() const;
State state() const;
int fd() const;
Buffer* input_buffer();
Buffer* output_buffer();
```

#### 发送与关闭

```cpp
void send(const void* data, std::size_t len);  // 线程安全
void send(const std::string& message);
void send(Buffer* buffer);   // 取走 buf 全部可读数据发送

void shutdown();      // 半关闭写端（发完 output_buffer 后 shutdown WR）
void force_close();   // 立即关闭

void set_tcp_no_delay(bool on);
```

`send` 在连接非 `kConnected` 时静默返回。数据先尝试直接 `write`，未写完则进入 `output_buffer` 并注册写事件。

#### 高水位回调

```cpp
void set_high_water_mark_callback(HighWaterMarkCallback cb, std::size_t mark);
// 默认 mark = 64 MB
```

当 `output_buffer` 从低于 mark 涨到 ≥ mark 时触发一次，用于背压控制。

#### 回调设置

```cpp
void set_connection_callback(ConnectionCallback cb);
void set_message_callback(MessageCallback cb);
void set_write_complete_callback(WriteCompleteCallback cb);
void set_close_callback(ConnectionCallback cb);  // 库内部使用，业务通过 TcpServer 间接设置
```

回调 setter **非线程安全**，见 [3.6](#36-回调与对象成员的线程安全)。须在所属 IO 线程、连接建立前设置。

---

### 5.8 TcpServer

高层 TCP 服务端入口。

#### 类型别名

```cpp
using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection>&, Buffer*, int64_t)>;
using WriteCompleteCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
using ThreadInitCallback = std::function<void(EventLoop*)>;
```

#### 构造与析构

```cpp
TcpServer(EventLoop* loop, const ::sockaddr_in& listen_addr, const std::string& name);
~TcpServer();  // 调 stop()，遵守 [3.3] 线程约束
```

- `loop`：主 `EventLoop`，负责 accept。
- `listen_addr`：监听地址，端口可为 `0`（由系统分配，通过 `port()` 获取）。

#### 启停

```cpp
void start();
void stop();   // 见 [3.3] 与 [4.1]
```

`stop()` 会：停止 listen → 在各 IO 线程内清空 `close_callback` 并强制关闭所有连接 → `join` 清理 IO 线程 → `thread_pool_.reset()` 释放线程池。

关停时清空 `close_callback` 的原因与残余竞态见 [3.6](#36-回调与对象成员的线程安全)。

**不可重启**：`stop()` 后无法再次 `start()`，须析构并重新构造 `TcpServer`（见 [3.4](#34-stop-后不可重启)）。

#### 配置

```cpp
void set_thread_num(int num_threads);  // start() 前

void set_connection_callback(ConnectionCallback cb);
void set_message_callback(MessageCallback cb);
void set_write_complete_callback(WriteCompleteCallback cb);
void set_thread_init_callback(ThreadInitCallback cb);
```

回调 setter **非线程安全**；须在 `start()` 前于主线程一次性设置，运行期勿跨线程修改，见 [3.6](#36-回调与对象成员的线程安全)。

`thread_init_callback` 在每个 IO 线程的 `EventLoop` 启动后、进入 `loop()` 前调用，可用于线程级初始化：

```cpp
server.set_thread_init_callback([](solar_net::EventLoop* io_loop) {
    // 在此线程绑定线程本地存储、预热资源等
});
```

#### 访问器

```cpp
EventLoop* get_loop() const;
std::shared_ptr<EventLoopThreadPool> thread_pool() const;
uint16_t port() const;
```

---

## 6. 回调语义

### 6.1 ConnectionCallback

```cpp
void(const std::shared_ptr<TcpConnection>& conn)
```

> **易混点**：同一回调在**建立**与**断开**时都会触发（`connection_established` 与 `handle_close` 均会调用）。业务侧**必须**用 `conn->state()` 区分，不可假定只在握手成功时进入回调。测试与线上常见坑：断开时误当新连接处理。

| 触发时机 | `conn->state()` | 调用路径 |
|----------|-----------------|----------|
| 连接建立完成 | `kConnected` | `connection_established()` |
| 连接断开（对端关闭、错误、`force_close` 等） | `kDisconnected` | `handle_close()` |

典型用法：建立时初始化会话状态，断开时清理资源。**两种分支都要写**，不要只处理 `kConnected`。

```cpp
server.set_connection_callback([](const TcpConnectionPtr& conn) {
    if (conn->state() == TcpConnection::State::kConnected) {
        // 新连接
    } else {
        // 连接已断开
    }
});
```

### 6.2 MessageCallback

```cpp
void(const std::shared_ptr<TcpConnection>& conn, Buffer* buf, int64_t receive_time)
```

- 在某次读事件读到数据后调用。
- `buf` 指向连接的 `input_buffer`，**不会自动消费**；须 `retrieve` 或 `retrieve_as_string`。
- `receive_time` 当前实现恒为 `0`（预留扩展，勿依赖具体值）。

### 6.3 WriteCompleteCallback

```cpp
void(const std::shared_ptr<TcpConnection>& conn)
```

当 `output_buffer` 全部写入内核发送缓冲区后调用（包括直接 `write` 成功与写事件刷完缓冲）。

### 6.4 回调执行线程

| 回调 | 执行线程 |
|------|----------|
| `TcpServer` 的 connection / message / write_complete | 该连接所在 IO 线程 |
| `thread_init_callback` | 对应 IO 线程（主线程 loop 不触发） |
| `Acceptor` 新连接回调 | 主 loop 线程 |

**所有回调中禁止**：长时间阻塞、同步 I/O、持锁等待其他线程、在回调里直接 `join` 其他线程的 `loop`。

---

## 7. 使用规范与反模式

### 7.1 必须遵守

1. **`loop.loop()` 返回后在 loop 所属线程调用 `server.stop()`**（见 [3.3]）。
2. **`set_thread_num` 在 `start()` 之前调用**。
3. **消息回调中处理粘包/半包**，及时 `retrieve` 已消费数据。
4. **回调中不做阻塞操作**，耗时逻辑投递到业务线程池。
5. **用 `conn->send()` 回写数据**，不要直接对 `fd` 操作。
6. **信号处理只调 `loop.stop()`**，清理放在 `loop.loop()` 返回之后。
7. **持有关连接状态的 `shared_ptr`** 时，注意循环引用；回调捕获 `this` 时用 `weak_ptr` 或确保生命周期。
8. **`TcpServer` 回调在 `start()` 前设置**，运行期勿跨线程 `set_*_callback`（见 [3.6]）。

### 7.2 推荐做法

```cpp
// 跨线程向指定连接发数据
conn->send("hello");  // 内部已 run_in_loop，无需手动投递

// 在 IO 线程向主 loop 汇报统计
main_loop->run_in_loop([&stats]() { stats.on_message(); });

// 测试/调试时绑定 127.0.0.1 随机端口
addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
addr.sin_port = htons(0);
server.start();
uint16_t port = server.port();
```

### 7.3 反模式

| 反模式 | 后果 |
|--------|------|
| `loop.loop()` 返回后，其他线程调 `server.stop()` | 关停任务不执行，资源泄漏 |
| `stop()` 后再 `start()` | 线程池已释放，无法恢复服务 |
| 在信号处理函数里 `server.stop()` | 异步安全与死锁风险 |
| 消息回调里 `sleep` / 同步查 DB | 阻塞 IO 线程，延迟所有连接 |
| 不 `retrieve` 导致 buffer 无限增长 | 内存上涨 |
| 同线程创建两个 `EventLoop` | `abort()` |
| `start()` 之后才 `set_thread_num` | 线程数不生效 |
| `start()` 后或运行中跨线程 `set_message_callback` 等 | 与 IO 线程读回调数据竞争；已连接仍用旧回调 |

---

## 8. 常见问题

### Q1：`send(Buffer* buf)` 会清空 buf 吗？

会。在 loop 线程直接 `retrieve_all`；跨线程则 `retrieve_all_as_string` 后投递。发送的是调用时刻的快照。

### Q2：单线程和多线程该如何选？

- 连接少、逻辑轻：`set_thread_num(0)`，调试简单。
- 多核、回调略重：`set_thread_num` 设为 CPU 核数或略少，连接按 round-robin 分配。

### Q3：如何实现定时踢空闲连接？

在 `connection_callback` 建立连接时：

```cpp
auto id = conn->get_loop()->run_every(60.0, [weak = std::weak_ptr<TcpConnection>(conn)]() {
    if (auto c = weak.lock()) {
        // 检查空闲时间，超时则 c->force_close();
    }
});
// 断开时 cancel(id)，需在会话结构中保存 TimerId
```

### Q4：`port()` 什么时候有效？

`Acceptor::listen()` 执行后（即 `start()` 之后）。监听 `0` 端口时返回系统分配的实际端口。

### Q5：客户端断开如何感知？

`connection_callback` 第二次调用且 `state() == kDisconnected`，或 `message_callback` 读至 EOF 后触发断开流程。

### Q6：与 muduo 的差异？

整体思路相近。本库体量更小，错误处理与日志较简，无内置日志宏；`receive_time` 尚未实现；仅 Linux。定时器 cancel 在 add 尚未入队时的竞态已通过 `pending_cancel_timers_` 修复（muduo 同类窗口未处理）。

---

## 附录：头文件速查

```cpp
#include "buffer.h"                  // Buffer
#include "socket.h"                  // Socket
#include "event_loop.h"              // EventLoop, TimerId
#include "timer.h"                   // TimerCallback, Timer, TimerId
#include "timestamp.h"               // Timestamp, now(), add_time()
#include "event_loop_thread.h"       // EventLoopThread
#include "event_loop_thread_pool.h"  // EventLoopThreadPool
#include "tcp_connection.h"          // TcpConnection, TcpConnectionPtr
#include "tcp_server.h"              // TcpServer
```

完整可运行示例：`example/echo_server.cpp`。
