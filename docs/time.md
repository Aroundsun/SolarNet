# Time

`Time` 是基于 `std::chrono::system_clock` 的时间戳值类型，提供格式化、比较与毫秒转换。被 `Logger`、`Channel`、`TimerQueue` 等模块广泛使用。

## 1. 职责

- 封装系统时间点，避免直接暴露 `time_point`。
- 提供日志格式、ISO 8601、自定义 strftime 格式化。
- 支持 `<=>` 三路比较，可用于定时器排序。
- 无共享可变状态，只读访问可并发。

## 2. 类图与生命周期

```
+---------------------------+
| Time                      |
+---------------------------+
| - m_time_point: TimePoint |
+---------------------------+
| + Now() -> Time           |
| + GetTimePoint()          |
| + MillisecondsSinceEpoch()|
| + ToIso8601()             |
| + ToFormattedString(fmt)  |
| + ToLogString()           |
| + operator<=>(Time)       |
+---------------------------+
```

生命周期：

1. **默认构造**：表示最小时间点（哨兵值）。
2. **`Time::Now()`**：捕获当前系统时间。
3. **显式构造**：从 `TimePoint` 构造，供定时器内部使用。
4. **使用**：只读访问，无析构副作用。

## 3. API

```cpp
namespace solar_net {

class Time {
 public:
  using Clock = std::chrono::system_clock;
  using TimePoint = Clock::time_point;

  static Time Now() noexcept;
  Time() = default;
  explicit Time(TimePoint tp) noexcept;

  TimePoint GetTimePoint() const noexcept;
  std::chrono::milliseconds MillisecondsSinceEpoch() const noexcept;

  std::string ToIso8601() const;
  std::string ToFormattedString(const std::string& format = "%Y-%m-%d %H:%M:%S") const;
  std::string ToLogString() const;

  std::strong_ordering operator<=>(const Time&) const noexcept = default;
};

}  // namespace solar_net
```

头文件：`#include "solar_net/base/time.h"`

## 4. 关键流程

### 日志时间格式化

```
LogEvent 构造
   | Timestamp = Time::Now()
   | DefaultFormatter
   |   event.Timestamp().ToLogString()
   |<-- "YYYY-MM-DD HH:MM:SS.mmm"
```

### 定时器排序

```
TimerQueue::RunAt(time, cb)
   | ActiveTimerSet 按 (Time, TimerId) 排序
   | ResetTimerFd(最早到期 Time)
```

## 5. 设计要点

- **值类型**：可拷贝、可比较，适合作为函数参数和容器键。
- **本地时区**：`ToFormattedString` / `ToLogString` 使用本地时区（strftime）。
- **Stable API**：Phase 1 起接口稳定，后续网络层直接复用。
- **与 steady_clock 分离**：定时器到期计算在 `TimerQueue` 内基于 `Time`（system_clock），与 muduo 一致。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| 默认构造的 Time | 表示 `TimePoint::min()`，仅作哨兵。 |
| 系统时钟回拨 | `TimerQueue` 依赖 system_clock，回拨可能导致定时器延迟（与 muduo 相同）。 |
| 空 format 串 | 走 strftime 默认行为。 |

## 7. 测试覆盖

- `NowReturnsCurrentTime`：`Now()` 与当前时间接近。
- `MillisecondsSinceEpochIsPositive`：纪元毫秒数为正。
- `LogStringContainsDateAndTime`：日志格式含日期与时间。
- `FormattedStringUsesFormat`：自定义 format 生效。
- `ComparisonWorks`：`<=>` 比较正确。

## 8. 示例

```cpp
#include "solar_net/base/time.h"

#include <iostream>

int main() {
    const solar_net::Time now = solar_net::Time::Now();
    std::cout << now.ToLogString() << '\n';
    std::cout << now.ToIso8601() << '\n';
    return 0;
}
```

## 9. 性能

- `Now()`：一次系统调用，O(1)。
- 格式化：涉及 strftime / 字符串分配，适合低频日志与定时器，不适合热路径逐字节格式化。

无独立 Benchmark；性能在 `bench_logger` 中间接体现。

## 10. 下一步

- [Logger](logger.md) 用 `Time::ToLogString()` 输出时间戳。
- [TimerQueue](timer_queue.md) 用 `Time` 维护到期顺序。
- [Channel](channel.md) 读回调携带事件到达时间 `Time`。
