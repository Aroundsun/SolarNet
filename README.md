# SolarNet

Linux 下的 C++ TCP 网络库，Reactor 模型，底层用 epoll。

整体思路参考 muduo：一个线程一个 `EventLoop`，fd 的读写事件挂在 `Channel` 上，业务侧通过回调处理连接和数据。目前能跑通 accept → 建连 → 收发包这条链路，带单测，附带一个 echo 示例程序。

## 环境

- Linux（依赖 epoll、eventfd）
- C++17
- CMake >= 3.14
- g++ 或 clang++

## 编译

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物：

- `build/libsolar_net.a` — 静态库
- `build/echo_server` — echo 示例

头文件在 `src/` 下，include 路径指向这个目录即可。

## 跑 demo

先启动 echo 服务：

```bash
./build/echo_server              # 默认 8080 端口，单线程
./build/echo_server -p 9000 -t 2 # 9000 端口，2 个 IO 线程
```

另开终端测试：

```bash
echo hello | nc 127.0.0.1 8080
# hello

nc 127.0.0.1 8080
# 输入什么回显什么，Ctrl+C 或 Ctrl+D 退出
```

服务端会打印连接建立/断开日志，Ctrl+C 退出。

参数：

| 选项 | 含义 | 默认 |
|------|------|------|
| `-p` / `--port` | 监听端口 | 8080 |
| `-t` / `--threads` | IO 线程数 | 0（单线程） |

## 跑测试

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

测试用 Google Test，CMake 第一次配置时会自动拉取。

## 线程模型

```
主线程 EventLoop
  ├── Acceptor 监听、accept 新连接
  └── EventLoopThreadPool（可选）
        ├── IO 线程 1 → EventLoop
        ├── IO 线程 2 → EventLoop
        └── ...
```

- `set_thread_num(0)`：单线程，accept 和连接读写都在主 loop 里。
- `set_thread_num(N)`：主 loop 只负责 accept，新连接 round-robin 分到 N 个 IO 线程的 loop 上。

跨线程调接口走 `run_in_loop` / `queue_in_loop`，内部用 eventfd 唤醒阻塞在 epoll 上的 loop。

## 模块

| 文件 | 作用 |
|------|------|
| `Buffer` | 读写缓冲区，带 cheap prepend，支持 `readv` 读 fd |
| `Socket` | fd 封装，非阻塞、TCP 选项等静态方法 |
| `Channel` | 把 fd 和 epoll 事件、回调绑在一起 |
| `EpollPoller` | epoll 封装，ADD/MOD/DEL |
| `EventLoop` | 事件循环，任务队列 + wakeup |
| `EventLoopThread` | 在独立线程里跑一个 EventLoop |
| `EventLoopThreadPool` | IO 线程池，轮询分发 loop |
| `Acceptor` | 监听 socket，accept 新连接 |
| `TcpConnection` | 已建立连接的读写、状态、output buffer 排队发送 |
| `TcpServer` | 把上面这些拼起来 |

`Channel`、`EpollPoller`、`TcpConnection` 的操作都要求在所属 loop 线程里调用（内部有 `assert_in_loop_thread`）。`TcpConnection::send` 和 `EventLoop::stop` 是线程安全的。

## 自己写服务端

`example/echo_server.cpp` 是最小完整示例，核心就这些：

```cpp
#include <arpa/inet.h>
#include "event_loop.h"
#include "tcp_connection.h"
#include "tcp_server.h"

solar_net::EventLoop loop;

sockaddr_in addr{};
addr.sin_family = AF_INET;
addr.sin_addr.s_addr = htonl(INADDR_ANY);
addr.sin_port = htons(8080);

solar_net::TcpServer server(&loop, addr, "my-server");
server.set_thread_num(2);

server.set_message_callback([](const solar_net::TcpConnectionPtr& conn,
                               solar_net::Buffer* buf,
                               int64_t) {
    conn->send(buf);  // 原样发回
});

server.start();
loop.loop();
```

回调在对应连接的 IO 线程里执行，别在里面做阻塞操作。

## 目录结构

```
src/          库源码
tests/        单测
example/      示例程序
CMakeLists.txt
```

## 测试覆盖

| 测试 | 内容 |
|------|------|
| `test_buffer` | Buffer 读写、扩容、read_from_fd |
| `test_socket` | socket 选项、地址获取 |
| `test_channel` | 事件分发、enable/disable、tie |
| `test_event_loop` | loop 运行、跨线程任务投递 |
| `test_tcp_connection` | 连接读写、跨线程 send、关闭 |
| `test_acceptor` | 监听、accept |
| `test_event_loop_thread` | IO 线程启动与任务调度 |
| `test_event_loop_thread_pool` | 线程池、round-robin |
| `test_tcp_server` | 服务端端到端 |

## 还没做的

- 没有 benchmark / 压测工具
- 没有 TLS、UDP、定时器
- 错误处理偏简单，生产用还需要补日志和更细的 errno 处理
- 只测过 Linux，没考虑 macOS / Windows

## License

未指定。如需开源请自行补充 license 文件。
