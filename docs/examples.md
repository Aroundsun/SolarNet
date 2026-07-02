# 示例程序

构建后可在 `build/examples/` 目录运行。

## example_version

打印项目名与语义化版本号。

```bash
./build/examples/example_version
# SolarNet 0.1.0
```

## example_logger

演示分级日志宏与 std::format 格式化输出。

```bash
./build/examples/example_logger
```

输出包含 DEBUG/INFO/WARN/ERROR 及源码位置。

## example_buffer

演示 HTTP 请求缓冲、CRLF 行查找、Prepend 长度头与十六进制转储。

```bash
./build/examples/example_buffer
```

## example_thread_pool

演示 4 线程池并发执行 20 个计数任务。

```bash
./build/examples/example_thread_pool
# 20 tasks finished, counter = 20
```

## example_channel

演示 Channel 读/写/关闭/错误回调分发。

```bash
./build/examples/example_channel
# read=1 write=1 close=1 error=1
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
