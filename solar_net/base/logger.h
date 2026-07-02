#pragma once

#include "solar_net/base/non_copyable.h"
#include "solar_net/base/time.h"

#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <atomic>
#include <source_location>
#include <string>
#include <vector>

namespace solar_net {

/** @brief 日志级别，数值越小越详细。线程安全（枚举常量）。 */
enum class LogLevel {
    kTrace = 0, ///< 最详细，用于追踪执行路径
    kDebug,     ///< 调试信息
    kInfo,      ///< 常规运行信息
    kWarn,      ///< 潜在问题警告
    kError,     ///< 错误
    kFatal,     ///< 致命错误
    kOff,       ///< 关闭日志输出
};

/** @brief 将日志级别转为字符串标签。线程安全。 */
[[nodiscard]] constexpr const char* LogLevelToString(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::kTrace: return "TRACE";
    case LogLevel::kDebug: return "DEBUG";
    case LogLevel::kInfo: return "INFO";
    case LogLevel::kWarn: return "WARN";
    case LogLevel::kError: return "ERROR";
    case LogLevel::kFatal: return "FATAL";
    case LogLevel::kOff: return "OFF";
    }
    return "UNKNOWN";
}

/**
 * @brief 单条日志的结构化快照（时间、级别、消息、源码位置、线程 ID）。
 *
 * 构造后字段不变。线程安全：const 访问可并发。
 */
class LogEvent {
public:
    /** @brief 构造日志事件，时间戳与线程 ID 在构造时捕获。线程安全。 */
    LogEvent(LogLevel level, std::string message, std::source_location location);

    /** @brief 返回日志级别。线程安全。 */
    [[nodiscard]] LogLevel Level() const noexcept { return m_level; }
    /** @brief 返回日志消息。线程安全。 */
    [[nodiscard]] const std::string& Message() const noexcept { return m_message; }
    /** @brief 返回事件发生时间。线程安全。 */
    [[nodiscard]] const Time& Timestamp() const noexcept { return m_timestamp; }
    /** @brief 返回源码位置（file/line/function）。线程安全。 */
    [[nodiscard]] std::source_location Location() const noexcept { return m_location; }
    /** @brief 返回写入线程的哈希 ID。线程安全。 */
    [[nodiscard]] uint64_t ThreadId() const noexcept { return m_thread_id; }

private:
    Time m_timestamp;
    LogLevel m_level;
    std::string m_message;
    std::source_location m_location;
    uint64_t m_thread_id{};
};

/**
 * @brief 将 LogEvent 格式化为输出字符串的抽象接口。
 *
 * 线程安全：取决于具体实现；调用方应保证实现本身可重入或在外部同步。
 */
class Formatter {
public:
    virtual ~Formatter() = default;
    /** @brief 格式化单条日志事件。 */
    [[nodiscard]] virtual std::string Format(const LogEvent& event) const = 0;
};

/** @brief 默认格式化器，输出 "时间 [级别] 文件:行 函数() - 消息"。线程安全（无状态）。 */
class DefaultFormatter : public Formatter {
public:
    [[nodiscard]] std::string Format(const LogEvent& event) const override;
};

/**
 * @brief 日志输出目标抽象接口。
 *
 * 线程安全：取决于具体实现；通过 Logger 调用时在 Logger 锁内串行执行。
 */
class Appender {
public:
    virtual ~Appender() = default;
    /** @brief 追加一条已格式化的日志行。 */
    virtual void Append(const std::string& log) = 0;
    /** @brief 刷新输出缓冲。 */
    virtual void Flush() = 0;
};

/** @brief 写入 std::cout 的输出器。线程安全：否（直接写标准输出，需外部同步）。 */
class ConsoleAppender : public Appender {
public:
    void Append(const std::string& log) override;
    void Flush() override;
};

/** @brief 追加写入文件的输出器。线程安全：否（直接写 ofstream，需外部同步）。 */
class FileAppender : public Appender {
public:
    /** @brief 以追加模式打开文件。线程安全：否。 */
    explicit FileAppender(const std::string& filename);
    ~FileAppender() override;

    void Append(const std::string& log) override;
    void Flush() override;

private:
    std::ofstream m_file;
};

/**
 * @brief 全局单例日志器，支持级别过滤、格式化与多路输出。
 *
 * 线程安全：Log / SetFormatter / AddAppender / ClearAppenders / Flush 在内部互斥锁保护下安全；
 * SetLevel / IsLevelEnabled 使用 atomic，与 Log 并发安全。
 */
class Logger : public NonCopyable {
public:
    /** @brief 返回进程内唯一 Logger 实例。线程安全（C++11 静态局部初始化）。 */
    [[nodiscard]] static Logger& GetInstance();

    /** @brief 写入一条日志（低于当前级别的会被丢弃）。线程安全。 */
    void Log(LogLevel level, std::source_location location, std::string message);

    /** @brief 按 std::format 语法写入一条日志。线程安全。 */
    template <typename... Args>
    void LogFormat(LogLevel level,
                   std::source_location location,
                   std::format_string<Args...> fmt,
                   Args&&... args) {
        Log(level, location, std::format(fmt, std::forward<Args>(args)...));
    }

    /** @brief 设置最低输出级别。线程安全。 */
    void SetLevel(LogLevel level) noexcept;

    /** @brief 判断给定级别是否会被输出。线程安全。 */
    [[nodiscard]] bool IsLevelEnabled(LogLevel level) const noexcept;

    /** @brief 替换格式化器。线程安全。 */
    void SetFormatter(std::unique_ptr<Formatter> formatter);

    /** @brief 追加一个输出器。线程安全。 */
    void AddAppender(std::unique_ptr<Appender> appender);

    /** @brief 移除所有输出器。线程安全。 */
    void ClearAppenders();

    /** @brief 刷新所有输出器缓冲。线程安全。 */
    void Flush();

private:
    Logger();

    std::atomic<LogLevel> m_level{LogLevel::kInfo};
    std::unique_ptr<Formatter> m_formatter;
    std::vector<std::unique_ptr<Appender>> m_appenders;
    mutable std::mutex m_mutex;
};

} // namespace solar_net

/** @brief 写入 TRACE 级别日志（含源码位置）。线程安全：同 Logger::Log。 */
#define LOG_TRACE(message) \
    do { \
        auto& logger = solar_net::Logger::GetInstance(); \
        if (logger.IsLevelEnabled(solar_net::LogLevel::kTrace)) { \
            logger.Log(solar_net::LogLevel::kTrace, std::source_location::current(), (message)); \
        } \
    } while (false)

/** @brief 写入 DEBUG 级别日志（含源码位置）。线程安全：同 Logger::Log。 */
#define LOG_DEBUG(message) \
    do { \
        auto& logger = solar_net::Logger::GetInstance(); \
        if (logger.IsLevelEnabled(solar_net::LogLevel::kDebug)) { \
            logger.Log(solar_net::LogLevel::kDebug, std::source_location::current(), (message)); \
        } \
    } while (false)

/** @brief 写入 INFO 级别日志（含源码位置）。线程安全：同 Logger::Log。 */
#define LOG_INFO(message) \
    do { \
        auto& logger = solar_net::Logger::GetInstance(); \
        if (logger.IsLevelEnabled(solar_net::LogLevel::kInfo)) { \
            logger.Log(solar_net::LogLevel::kInfo, std::source_location::current(), (message)); \
        } \
    } while (false)

/** @brief 写入 WARN 级别日志（含源码位置）。线程安全：同 Logger::Log。 */
#define LOG_WARN(message) \
    do { \
        auto& logger = solar_net::Logger::GetInstance(); \
        if (logger.IsLevelEnabled(solar_net::LogLevel::kWarn)) { \
            logger.Log(solar_net::LogLevel::kWarn, std::source_location::current(), (message)); \
        } \
    } while (false)

/** @brief 写入 ERROR 级别日志（含源码位置）。线程安全：同 Logger::Log。 */
#define LOG_ERROR(message) \
    do { \
        auto& logger = solar_net::Logger::GetInstance(); \
        if (logger.IsLevelEnabled(solar_net::LogLevel::kError)) { \
            logger.Log(solar_net::LogLevel::kError, std::source_location::current(), (message)); \
        } \
    } while (false)

/** @brief 写入 FATAL 级别日志（含源码位置）。线程安全：同 Logger::Log。 */
#define LOG_FATAL(message) \
    do { \
        auto& logger = solar_net::Logger::GetInstance(); \
        if (logger.IsLevelEnabled(solar_net::LogLevel::kFatal)) { \
            logger.Log(solar_net::LogLevel::kFatal, std::source_location::current(), (message)); \
        } \
    } while (false)
