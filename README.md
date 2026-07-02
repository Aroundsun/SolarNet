# SolarNet

受 muduo 启发的现代 C++20 Linux 网络框架，从零重构，面向长期维护与高性能。

## 当前状态

**Phase 2 完成**：基础模块（Time、Logger、Buffer）与并发原语（Thread、ThreadPool）已实现，含完整单测与 Benchmark。

| 模块 | 说明 |
| --- | --- |
| `Time` | 时间戳与格式化 |
| `Logger` | 分级日志 + 多 Appender |
| `Buffer` | 网络 I/O 缓冲区（readv/writev） |
| `Thread` | std::thread RAII 封装 |
| `ThreadPool` | 固定大小任务线程池 |
| `Channel` | I/O 事件通道 |
| `Poller` / `EpollPoller` | IO 多路复用（epoll） |
| `EventLoop` | Reactor 事件循环 |

详细设计见 [docs/architecture.md](docs/architecture.md)，评审报告见 [docs/review.md](docs/review.md)。

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
# 38 项测试
```

## 运行基准测试

```bash
./build/benchmarks/bench_buffer --benchmark_min_time=0.1s
./build/benchmarks/bench_thread_pool --benchmark_min_time=0.1s
./build/benchmarks/bench_version
```

## 运行示例

```bash
./build/examples/example_version
./build/examples/example_logger
./build/examples/example_buffer
./build/examples/example_thread_pool
./build/examples/example_channel
./build/examples/example_poller
./build/examples/example_event_loop
```

示例说明见 [docs/examples.md](docs/examples.md)。

## 开发选项

| CMake 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `SOLAR_NET_BUILD_TESTS` | ON | 构建单元测试 |
| `SOLAR_NET_BUILD_BENCHMARKS` | ON | 构建微基准测试 |
| `SOLAR_NET_BUILD_EXAMPLES` | ON | 构建示例程序 |
| `SOLAR_NET_BUILD_SHARED` | OFF | 构建动态库（默认静态库） |
| `SOLAR_NET_USE_CLANG_TIDY` | ON | 构建时运行 clang-tidy |

## 目录结构

```
SolarNet/
├── solar_net/           # 库源码（头文件与 .cpp 同目录）
│   ├── base/            # 基础模块
│   └── version.h
├── tests/               # GoogleTest 单元测试
├── benchmarks/          # Google Benchmark
├── examples/            # 示例程序
└── docs/                # 设计文档、评审报告
```

- `.clang-format` — 代码格式化
- `.clang-tidy` — 静态分析
- 所有代码须在开启的警告选项下干净编译通过

## 许可证

待定
