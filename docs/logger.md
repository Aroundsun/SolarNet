# Logger

`Logger` 是进程级单例日志器，支持级别过滤、可插拔 Formatter/Appender，以及带源码位置的 `LOG_*` 宏。是 SolarNet 的横切关注点，`ThreadPool` 等模块在异常路径依赖它。

## 1. 职责

- 按级别过滤并写入日志。
- 捕获时间戳、线程 ID、源码位置（`std::source_location`）。
- 支持多路输出（Console、File、自定义 Appender）。
- 提供 `LOG_TRACE` … `LOG_FATAL` 宏，级别不满足时零开销跳过格式化。

## 2. 类图与生命周期

```
+------------------+       +------------------+
| Logger (单例)     | uses  | Formatter        |
+------------------+       +------------------+
| - m_level        |       | + Format(event)  |
| - m_formatter    |       +------------------+
| - m_appenders[]  |              ^
| - m_mutex        |       +------+-----------+
+------------------+       | DefaultFormatter |
| + Log()          |       +------------------+
| + SetLevel()     |
| + AddAppender()  | uses  +------------------+
+------------------+       | Appender         |
                           +------------------+
                           | + Append(log)    |
                           | + Flush()        |
                           +------------------+
                                    ^
                           +--------+---------+
                           | ConsoleAppender  |
                           | FileAppender     |
                           +------------------+
```

生命周期：

1. **首次 `GetInstance()`**：Meyers 单例初始化，默认 `Info` 级别 + `DefaultFormatter` + `ConsoleAppender`。
2. **运行时**：`LOG_*` → `Log()` → 格式化 → 各 Appender 输出。
3. **进程退出**：静态析构顺序依赖实现，测试中用 TearDown 重置级别。

## 3. API

```cpp
namespace solar_net {

enum class LogLevel { kTrace, kDebug, kInfo, kWarn, kError, kFatal, kOff };

class Logger : NonCopyable {
 public:
  static Logger& GetInstance();

  void Log(LogLevel level, std::source_location location, std::string message);
  void SetLevel(LogLevel level) noexcept;
  bool IsLevelEnabled(LogLevel level) const noexcept;

  void SetFormatter(std::unique_ptr<Formatter> formatter);
  void AddAppender(std::unique_ptr<Appender> appender);
  void ClearAppenders();
  void Flush();
};

}  // namespace solar_net

// 宏：LOG_TRACE / LOG_DEBUG / LOG_INFO / LOG_WARN / LOG_ERROR / LOG_FATAL
```

头文件：`#include "solar_net/base/logger.h"`

## 4. 关键流程

### 写入流程

```
LOG_INFO("msg")
   | IsLevelEnabled(kInfo)?
   |   no  -> 跳过
   |   yes -> Log()
   |            构造 LogEvent(level, msg, source_location)
   |            formatter->Format(event)
   |            for each appender: Append(formatted)
```

### 级别过滤

```
SetLevel(kWarn)
   | LOG_INFO  -> 丢弃
   | LOG_WARN  -> 输出
   | LOG_ERROR -> 输出
```

## 5. 设计要点

- **单例 + 宏**：简化调用方代码，宏内先检查级别避免无谓 `std::format`。
- **扩展点**：`Formatter` / `Appender` 抽象类，可替换输出格式与目标。
- **线程安全**：`Log` / `AddAppender` 等在 mutex 内；`SetLevel` / `IsLevelEnabled` 用 `atomic`。
- **依赖 Time**：`LogEvent` 构造时捕获 `Time::Now()`。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| 无 Appender | 格式化后无人接收，日志丢弃。 |
| Appender 抛异常 | 会传播出 `Log()`，Appender 实现不应抛异常。 |
| 测试间共享单例 | 测试 TearDown 重置级别；可 `ClearAppenders` 换 mock。 |
| `kOff` 级别 | 所有级别均被过滤。 |

## 7. 测试覆盖

- `LogInfoStoresMessage`：INFO 日志含消息与级别标签。
- `DebugIsFilteredWhenLevelIsInfo`：级别过滤生效。
- `LogLevelOrderWorks`：Warn 以上才输出。
- `FileAppenderWritesToFile`：文件输出正确。
- `LoggerIsNonCopyable`：继承 `NonCopyable`，不可拷贝。

## 8. 示例

```cpp
#include "solar_net/base/logger.h"

#include <format>

int main() {
    solar_net::Logger::GetInstance().SetLevel(solar_net::LogLevel::kDebug);
    LOG_INFO("server started");
    LOG_INFO(std::format("port = {}", 8080));
    return 0;
}
```

运行：

```bash
./build/examples/example_logger
```

## 9. 性能

- 宏级别检查：atomic 读，热路径可接受。
- 完整写入：mutex + 格式化 + IO，适合中低频；异步 Appender 为后续演进方向。

无独立 Benchmark。

## 10. 下一步

- 异步 Appender、日志轮转。
- [ThreadPool](thread_pool.md) 任务异常已通过 `LOG_ERROR` 记录。
- 网络层连接/错误日志将统一走 Logger。
