# InetAddress

`InetAddress` 是 SolarNet Phase 4 Transport Layer 的第一个模块。它统一封装 `sockaddr_in` / `sockaddr_in6`，为 `Socket`、`Acceptor`、`TcpConnection` 提供与地址族无关的地址对象。

## 1. 职责

- 隐藏 `sockaddr` 细节，上层代码无需直接操作 `sockaddr_in` / `sockaddr_in6`。
- 同时支持 IPv4 与 IPv6（IPv6 接口已预留，可无缝使用）。
- 提供文本形式（`ToIp`、`ToIpPort`）与内核形式（`GetSockAddr`、`GetSockLen`）双向转换。
- 作为值类型，可拷贝、可比较，能直接用作 `std::map` / `std::set` 的键。

## 2. 类图与生命周期

```
+---------------------------+
| InetAddress               |
+---------------------------+
| - m_addr: sockaddr_storage|
| - m_len: socklen_t        |
+---------------------------+
| + InetAddress()           |
| + InetAddress(port)       |
| + InetAddress(ip, port)   |
| + InetAddress(sockaddr*)  |
| + Family()                |
| + ToIp()                  |
| + ToIpPort()              |
| + Port()                  |
| + GetSockAddr()           |
| + GetSockLen()            |
| + operator==()            |
| + operator<()             |
+---------------------------+
```

生命周期：

1. **构造**：从端口、IP+端口或已有的 `sockaddr` 初始化内部 `sockaddr_storage`。
2. **使用**：以只读方式获取地址信息或转换为字符串。
3. **拷贝/移动**：值类型，默认生成。
4. 析构无需手动资源释放。

## 3. API

```cpp
namespace solar_net {

class InetAddress {
public:
    InetAddress() noexcept;

    explicit InetAddress(uint16_t port,
                         bool loopback_only = false,
                         sa_family_t family = AF_INET);

    explicit InetAddress(std::string_view ip,
                         uint16_t port,
                         sa_family_t family = AF_INET);

    explicit InetAddress(const sockaddr_in& addr) noexcept;
    explicit InetAddress(const sockaddr_in6& addr) noexcept;
    explicit InetAddress(const sockaddr_storage& addr) noexcept;
    explicit InetAddress(const sockaddr* addr, socklen_t len);

    [[nodiscard]] sa_family_t Family() const noexcept;
    [[nodiscard]] std::string ToIp() const;
    [[nodiscard]] std::string ToIpPort() const;
    [[nodiscard]] uint16_t Port() const noexcept;

    [[nodiscard]] const sockaddr* GetSockAddr() const noexcept;
    [[nodiscard]] sockaddr* GetSockAddr() noexcept;
    [[nodiscard]] socklen_t GetSockLen() const noexcept;

    [[nodiscard]] bool operator==(const InetAddress& other) const noexcept;
    [[nodiscard]] bool operator!=(const InetAddress& other) const noexcept;
    [[nodiscard]] bool operator<(const InetAddress& other) const noexcept;
};

} // namespace solar_net
```

## 4. 关键流程

### 从 IP + Port 构造

```
Caller
   | InetAddress("192.168.1.1", 8080)
   |   ResetToAny(AF_INET)
   |   inet_pton(AF_INET, "192.168.1.1", &sin_addr)
   |   sin_port = htons(8080)
   |   m_len = sizeof(sockaddr_in)
   |<-- InetAddress
```

### 获取 sockaddr 用于 bind/accept/connect

```
Caller
   | addr.GetSockAddr()
   |<-- const sockaddr*

Caller
   | addr.GetSockLen()
   |<-- socklen_t
```

## 5. 设计要点

- **值语义**：不继承 `NonCopyable`，默认拷贝/移动，适合在回调和容器中传递。
- **统一存储**：内部使用 `sockaddr_storage`，容量足够容纳 IPv4 与 IPv6。
- **长度跟踪**：`m_len` 记录实际地址长度，避免向内核传递多余字节。
- **字节序透明**：端口在内部始终以网络字节序存储，外部 `Port()` 返回主机字节序。
- **错误回退**：`inet_pton` 失败时记录日志并回退到 `0.0.0.0:0`，不抛异常。
- **比较安全**：`operator<` 先比较 `sa_family`，避免不同族地址因内存布局差异导致错误排序。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| 非法 IP 字符串 | 记录 `LOG_ERROR`，回退到 `0.0.0.0:0`。 |
| 不支持的地址族 | 记录 `LOG_ERROR`，回退到 `0.0.0.0:0`。 |
| `nullptr` sockaddr | 回退到 `0.0.0.0:0`。 |
| IPv6 字符串格式 | `ToIpPort()` 输出 `[ip]:port`，符合 URI 惯例。 |
| 拷贝大量地址 | `sockaddr_storage` 固定 128 字节，拷贝成本可预期。 |

## 7. 测试覆盖

- 默认构造为 `0.0.0.0:0`。
- 从端口构造（any / loopback）。
- 从 IP + 端口构造。
- 从 `sockaddr_in`、`sockaddr_storage`、`sockaddr*` 构造。
- `ToIp` / `ToIpPort` / `Port` 正确性。
- 相等、不等、小于比较。
- 作为 `std::map` 键使用。
- 拷贝语义。
- 非法输入回退。
- IPv6 构造与格式化。

## 8. 示例

```cpp
#include "solar_net/inet_address.h"

#include <format>
#include <iostream>

int main() {
    const solar_net::InetAddress any(8080);
    std::cout << std::format("any:      {}\n", any.ToIpPort());

    const solar_net::InetAddress loopback(8080, true);
    std::cout << std::format("loopback: {}\n", loopback.ToIpPort());

    const solar_net::InetAddress ipv4("192.168.1.1", 8080);
    std::cout << std::format("ipv4:     {}\n", ipv4.ToIpPort());

    const solar_net::InetAddress ipv6(8080, true, AF_INET6);
    std::cout << std::format("ipv6:     {}\n", ipv6.ToIpPort());

    return 0;
}
```

## 9. 性能

| 操作 | 复杂度 | 说明 |
|---|---|---|
| 构造（端口） | O(1) | 仅初始化固定内存。 |
| 构造（IP+端口） | O(1) | `inet_pton` 为常数时间。 |
| `ToIp` / `ToIpPort` | O(L) | L 为输出字符串长度，涉及 `inet_ntop`。 |
| `GetSockAddr` / `GetSockLen` | O(1) | 直接返回指针/长度。 |
| 比较 | O(1) | `memcmp` 固定长度。 |
| 内存占用 | 128 bytes | `sockaddr_storage` 大小。 |

## 10. 下一步

- 接入 `Socket` 模块：作为 `bind()`、`connect()`、`accept()` 的参数与返回值。
- 考虑支持 DNS 解析（`getaddrinfo`）的上层封装，但保持 `InetAddress` 自身不依赖 DNS。
- 后续可扩展 `IsMulticast()`、`IsLoopback()` 等辅助判断。
- 在 `TcpConnection` 中用于记录对端地址 `PeerAddress()` / 本地地址 `LocalAddress()`。
