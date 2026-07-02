# Version

`Version` 提供 SolarNet 的项目名与语义化版本号常量，供示例、测试与 CI 校验使用。

## 1. 职责

- 集中定义主版本、次版本、补丁号。
- 提供 `ProjectName()` 与 `VersionString()` 字符串视图。
- 编译期常量，无运行时状态。

## 2. 类图与生命周期

```
+---------------------------+
| Version (struct)          |
+---------------------------+
| + kMajor / kMinor / kPatch|
| + ProjectName()           |
| + VersionString()         |
+---------------------------+
```

无实例生命周期；全部为 `static constexpr` 接口。

## 3. API

```cpp
namespace solar_net {

struct Version {
  static constexpr int kMajor = 0;
  static constexpr int kMinor = 1;
  static constexpr int kPatch = 0;

  static constexpr std::string_view ProjectName() noexcept;
  static constexpr std::string_view VersionString() noexcept;
};

}  // namespace solar_net
```

头文件：`#include "solar_net/version.h"`

## 4. 关键流程

```
example_version / test_version
   | Version::ProjectName()
   | Version::VersionString()
   |<-- "SolarNet 0.1.0"
```

## 5. 设计要点

- **Stable API**：版本结构随发布变更，接口本身不变。
- **string_view**：避免动态分配，适合 constexpr 上下文。
- **与 CMake/project 同步**：发版时需同时更新此处与打包脚本。

## 6. 边界情况

| 场景 | 处理 |
|---|---|
| 字符串不含 '\0' 问题 | `string_view` 不保证以 null 结尾，打印时用 `cout << view`。 |

## 7. 测试覆盖

- `ProjectNameIsSolarNet`
- `VersionStringIsCorrect`
- `SemanticVersionMatches`：`kMajor.kMinor.kPatch` 与 `VersionString()` 一致。

## 8. 示例

```cpp
#include "solar_net/version.h"

#include <iostream>

int main() {
    std::cout << solar_net::Version::ProjectName() << ' '
              << solar_net::Version::VersionString() << '\n';
    return 0;
}
```

运行：

```bash
./build/examples/example_version
# SolarNet 0.1.0
```

## 9. 性能

编译期常量，零运行时开销。

Benchmark：`bench_version`（占位/编译期校验）。

## 10. 下一步

- 发版时递增 `kPatch` / `kMinor`。
- 可选：从 CMake `project(VERSION)` 生成头文件，避免手工双写。
