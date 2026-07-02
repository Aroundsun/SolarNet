# 示例程序

构建后可在 `build/examples/` 目录运行。各模块设计见 [README.md](README.md)。

## example_version

打印项目名与语义化版本号。见 [version.md](version.md)。

```bash
./build/examples/example_version
# SolarNet 0.1.0
```

## example_logger

演示分级日志宏与 std::format 格式化输出。见 [logger.md](logger.md)。

```bash
./build/examples/example_logger
```

输出包含 DEBUG/INFO/WARN/ERROR 及源码位置。

## example_buffer

演示 HTTP 请求缓冲、CRLF 行查找、Prepend 长度头与十六进制转储。见 [buffer.md](buffer.md)。

```bash
./build/examples/example_buffer
```

## example_thread_pool

演示 4 线程池并发执行 20 个计数任务。见 [thread_pool.md](thread_pool.md)。

```bash
./build/examples/example_thread_pool
# 20 tasks finished, counter = 20
```

## example_channel

演示 Channel 读/写/关闭/错误回调分发。见 [channel.md](channel.md)。

```bash
./build/examples/example_channel
# read=1 write=1 close=1 error=1
```

## example_poller

演示 EpollPoller 与 pipe 可读事件。见 [poller.md](poller.md)。

```bash
./build/examples/example_poller
```

## example_event_loop

演示单线程 Reactor：`RunInLoop`、`QueueInLoop` 与跨线程 `Quit`。见 [event_loop.md](event_loop.md)。

```bash
./build/examples/example_event_loop
```

## example_timer_queue

演示 `RunAfter`、`RunEvery` 与 `Cancel`。见 [timer_queue.md](timer_queue.md)。

```bash
./build/examples/example_timer_queue
```

## example_event_loop_thread

演示在后台线程运行 `EventLoop`，通过 `RunInLoop` 注册定时器。见 [event_loop_thread.md](event_loop_thread.md)。

```bash
./build/examples/example_event_loop_thread
```

## example_event_loop_thread_pool

演示 3 线程 IO 池，各 loop 独立注册定时器。见 [event_loop_thread_pool.md](event_loop_thread_pool.md)。

```bash
./build/examples/example_event_loop_thread_pool
```

## example_acceptor

在 EventLoopThread 上监听 8080 并接受连接。见 [acceptor.md](acceptor.md)。

```bash
./build/examples/example_acceptor
# 另开终端: nc 127.0.0.1 8080
```

## example_inet_address

演示 IPv4/IPv6 地址格式化。见 [inet_address.md](inet_address.md)。

```bash
./build/examples/example_inet_address
```

## example_socket

演示 Socket 创建、bind/listen。见 [socket.md](socket.md)。

```bash
./build/examples/example_socket
```

## example_tcp_connection

Acceptor + TcpConnection 回声服务器，监听 12345。见 [tcp_connection.md](tcp_connection.md)。

```bash
./build/examples/example_tcp_connection
# 另开终端: nc 127.0.0.1 12345
```

## example_tcp_server

多线程 TCP 回声服务器，支持指定端口与 IO 线程数。见 [tcp_server.md](tcp_server.md)。

```bash
./build/examples/example_tcp_server
./build/examples/example_tcp_server 8080 4
# 另开终端: nc 127.0.0.1 12345
```

## 典型用法片段

### Logger

```cpp
#include "solar_net/base/logger.h"

solar_net::Logger::GetInstance().SetLevel(solar_net::LogLevel::kDebug);
LOG_INFO("server started on port 8080");
```

### Buffer

```cpp
#include "solar_net/base/buffer.h"

solar_net::Buffer buf;
buf.Append("GET / HTTP/1.1\r\n");
const auto* line = buf.FindCRLF();
if (line != buf.ReaderBegin() + buf.ReadableBytes()) {
    buf.RetrieveUntil(line + 2);  // 跳过 \r\n
}
```

### ThreadPool

```cpp
#include "solar_net/base/thread_pool.h"

solar_net::ThreadPool pool(4, "worker");
pool.Start();
pool.Submit([] { /* task */ });
pool.Stop();
pool.Wait();
```
