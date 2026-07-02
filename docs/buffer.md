# Buffer

`Buffer` 是面向网络 I/O 的可扩容连续字节缓冲区，采用 muduo 风格三区布局 `[prepend | readable | writable]`，支持零拷贝读取、整数编解码与 `readv`/`writev` 系统调用。

## 1. 职责

- 在单线程上下文中缓冲收发数据。
- 自动扩容与内部搬移，减少不必要分配。
- 提供 HTTP 风格的 CRLF/LF 行查找。
- 支持 Prepend 长度头等协议常见操作。
- 通过 `ReadFd` / `WriteFd` 与 socket 对接。

## 2. 类图与生命周期

```
+----------------------------------+
| Buffer                           |
+----------------------------------+
| - m_data: vector<byte>           |
| - m_reader_index: size_t         |
| - m_writer_index: size_t         |
+----------------------------------+
| + Append / Retrieve / Peek       |
| + AppendInt* / ReadInt*          |
| + FindCRLF / FindLF              |
| + ReadFd / WriteFd               |
| + Prepend / PrependInt32         |
| - MakeSpace / EnsureWritableBytes|
+----------------------------------+
```

内存布局：

```
| prepended |     readable     |    writable    |
|  (8B 默认) |   [reader, writer)|  [writer, end) |
```

生命周期：

1. **构造**：分配默认 1024 字节可写区 + 8 字节 prepend 预留。
2. **读写**：Append 写入，Retrieve 消费；不足时 `MakeSpace` 搬移或扩容。
3. **析构**：`vector` 自动释放。

## 3. API

```cpp
namespace solar_net {

class Buffer {
 public:
  static constexpr size_t kInitialPrepend = 8;
  static constexpr size_t kInitialSize = 1024;

  Buffer();
  explicit Buffer(size_t initial_size);

  size_t ReadableBytes() const noexcept;
  size_t WritableBytes() const noexcept;
  size_t PrependableBytes() const noexcept;

  void Append(std::string_view data);
  void Retrieve(size_t len);
  void RetrieveAll();
  std::string RetrieveAsString(size_t len);

  std::span<const std::byte> ReadableSpan() const noexcept;
  const std::byte* FindCRLF() const;
  const std::byte* FindLF() const;

  void PrependInt32(int32_t value);
  ssize_t ReadFd(int fd, int* saved_errno);
  ssize_t WriteFd(int fd, int* saved_errno);

  // AppendInt* / ReadInt* / PeekInt* / ToHexString / Swap ...
};

}  // namespace solar_net
```

头文件：`#include "solar_net/base/buffer.h"`

## 4. 关键流程

### Append 扩容

```
Append(data)
   | EnsureWritableBytes(len)
   |   writable >= len? 直接写
   |   else MakeSpace: 搬移 readable 到 prepend 后，或扩容 vector
   | memcpy 到 WriterBegin()
   | m_writer_index += len
```

### ReadFd（readv）

```
ReadFd(fd)
   | 可写区 + 栈上 extrabuf
   | readv(iovec[2])
   | 若 extrabuf 有数据 -> Append 到 Buffer
```

## 5. 设计要点

- **单线程**：无锁，由 `TcpConnection` 等上层保证同线程访问。
- **网络字节序**：多字节整数 Append/Read 使用 htons/ntoh 族。
- **零拷贝读**：`ReadableSpan()` / `ToStringView()` 不拷贝数据。
- **Shrink 空实现**：Phase 1 占位，后续可归还内存给 OS。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| Retrieve 超过可读长度 | assert 失败（debug 构建）。 |
| FindCRLF 未找到 | 返回可读区末尾指针。 |
| ReadFd 返回 0 | 对端关闭，由调用方处理。 |
| ReadFd EAGAIN | 返回 -1，errno 写入 saved_errno。 |
| Prepend 空间不足 | 先 MakeSpace 再 prepend。 |

## 7. 测试覆盖

- `DefaultConstructsEmpty` / `AppendAndRetrieveString` / `AutoExpansion`
- `RetrievePartial` / `AppendAndReadIntegers` / `PeekIntDoesNotAdvance`
- `FindCRLF` / `FindLF` / `PrependInt32`
- `ReadableSpanIsZeroCopy` / `RetrieveUntilDelimiter`
- `WriteFdAndReadFd` / `SwapExchangesContent`

## 8. 示例

```cpp
#include "solar_net/base/buffer.h"

#include <iostream>

int main() {
    solar_net::Buffer buffer;
    buffer.Append("GET / HTTP/1.1\r\n\r\n");

    const auto* line_end = buffer.FindCRLF();
    // ... 解析首行、PrependInt32 等

    std::cout << buffer.ToStringView() << '\n';
    return 0;
}
```

运行：

```bash
./build/examples/example_buffer
```

## 9. 性能

- Append/Retrieve：均摊 O(1)，扩容时 O(n) 搬移。
- ReadFd：readv 减少一次系统调用与拷贝。

Benchmark：`bench_buffer`，测试 Append/Retrieve/ReadFd 等热路径。

## 10. 下一步

- 实现 `Shrink()`、可选内存池。
- 与 `TcpConnection` 结合，作为输入/输出缓冲。
- 协议解析（HTTP 行、长度前缀）基于 `FindCRLF` / `PrependInt32` 构建。
