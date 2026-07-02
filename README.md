# SolarNet

受 muduo 启发的现代 C++20 Linux 网络框架，从零重构，面向长期维护与高性能。

## 当前状态

Phase 0：工程基础设施已就绪，项目可完成配置、构建、测试与基准测试。

## 构建要求

- CMake >= 3.20
- C++20 编译器（GCC >= 11 或 Clang >= 14）
- Linux 环境（WSL / Ubuntu 虚拟机 / 原生 Linux）
- 可选：`clang-tidy`、`ninja-build`
- 测试与基准测试依赖（系统包，推荐）：

```bash
sudo apt install libgtest-dev libbenchmark-dev
```

未安装上述包时，CMake 配置阶段会直接报错。

## 构建

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## 运行测试

```bash
ctest --test-dir build --output-on-failure
```

## 运行基准测试

```bash
./build/benchmarks/bench_version
```

## 运行示例

```bash
./build/examples/example_version
```

## 开发选项

| CMake 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `SOLAR_NET_BUILD_TESTS` | ON | 构建单元测试 |
| `SOLAR_NET_BUILD_BENCHMARKS` | ON | 构建微基准测试 |
| `SOLAR_NET_BUILD_EXAMPLES` | ON | 构建示例程序 |
| `SOLAR_NET_BUILD_SHARED` | OFF | 构建动态库（默认静态库） |
| `SOLAR_NET_USE_CLANG_TIDY` | ON | 构建时运行 clang-tidy |

## 目录结构

- `solar_net/` — 公共头文件与实现同目录（每个模块一对 `.h` / `.cpp`）。
- `tests/` — GoogleTest 单元测试。
- `benchmarks/` — Google Benchmark 微基准测试。
- `examples/` — 示例程序。

- `.clang-format` — 代码格式化
- `.clang-tidy` — 静态分析
- 所有代码须在开启的警告选项下干净编译通过。

## 许可证

待定
