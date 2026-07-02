# MEMORY.md - 长期记忆

## 用户档案

- **Name:** 邢浩毅（xhy）
- **身份:** 西安财经大学计算机科学与技术本科，预计 2026 年毕业，准备秋招。
- **方向:** C++ 后端开发。
- **城市:** 西安。
- **环境:** VMware 虚拟机，Ubuntu 26.04 LTS。

## 项目上下文

- **SolarVault（原 SolarDrive）**: C++17 云存储系统，基于 Reactor 多线程模型，含 JWT 认证、Redis 缓存、断点续传、Docker 容器化、WebSocket 实时通信。
- **SolarMcp**: MCP 工具服务器。
- **SolarNet**: 新建现代 C++20 网络框架，目标作为 Solar 系列底层网络基础设施，长期开源维护。
- **技术栈:** C++17/C++20、PostgreSQL、Redis、WebSocket、OpenSSL、Docker、Prometheus。

## 协作偏好

- 中文简洁交流，直接了当。
- **方案先行**：动手前必须讲清楚原理、方案对比、选择理由。
- 重视架构合理性，会主动质疑并纠正不合理设计。
- 重视输出视觉美观性，偏好图表解释技术概念。
- 会逐版本反馈，要求细化。

## SolarNet 项目约定

- **源码布局**: 公共头文件与实现文件放在同一目录 `solar_net/` 下，不按 `include/` 和 `src/` 拆分。每个模块对应 `xxx.h` + `xxx.cpp` 同目录存在，便于维护时一一对应。
- **技术栈**: C++20、CMake、clang-format、clang-tidy、GoogleTest、Google Benchmark。
- **开发流程**: 每模块走 12 步：需求分析 → 模块职责 → 接口设计 → 类图 → 生命周期 → 流程图 → 边界情况 → 性能分析 → 编码 → 单测 → Benchmark → 文档。
- **设计原则**: SOLID、RAII、KISS、DRY、高内聚低耦合、组合优于继承、接口优于实现；类 < 500 行、cpp < 800 行。
- **命名**: PascalCase 类、 `m_xxx` 成员、 `camelCase` 函数、 `kXXX` 常量、 `solar_net` namespace。
- **资源管理**: RAII，禁止裸 new/delete，优先 `unique_ptr`/`shared_ptr`/`span`/`string_view`/`expected`/`chrono`/`jthread`/`source_location`。

## 重要日期

- 2026-07-02: SolarNet 项目启动，确立开发流程与设计规范。
