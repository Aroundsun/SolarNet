#include "solar_net/base/logger.h"

#include <functional>
#include <thread>

namespace solar_net {

namespace {

uint64_t HashThreadId(std::thread::id id) noexcept {
    return std::hash<std::thread::id>{}(id);
}

} // namespace

LogEvent::LogEvent(LogLevel level, std::string message, std::source_location location)
    : m_timestamp(Time::Now()),
      m_level(level),
      m_message(std::move(message)),
      m_location(location),
      m_thread_id(HashThreadId(std::this_thread::get_id())) {}

std::string DefaultFormatter::Format(const LogEvent& event) const {
    return std::format("{} [{}] {}:{} {}() - {}",
                       event.Timestamp().ToLogString(),
                       LogLevelToString(event.Level()),
                       event.Location().file_name(),
                       event.Location().line(),
                       event.Location().function_name(),
                       event.Message());
}

void ConsoleAppender::Append(const std::string& log) {
    std::cout << log << '\n';
}

void ConsoleAppender::Flush() {
    std::cout.flush();
}

FileAppender::FileAppender(const std::string& filename) : m_file(filename, std::ios::app) {}

FileAppender::~FileAppender() = default;

void FileAppender::Append(const std::string& log) {
    if (!m_file.is_open()) {
        return;
    }
    m_file << log << '\n';
}

void FileAppender::Flush() {
    if (m_file.is_open()) {
        m_file.flush();
    }
}

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : m_formatter(std::make_unique<DefaultFormatter>()) {
    m_appenders.push_back(std::make_unique<ConsoleAppender>());
}

void Logger::Log(LogLevel level, std::source_location location, std::string message) {
    if (!IsLevelEnabled(level)) {
        return;
    }

    LogEvent event{level, std::move(message), location};

    std::lock_guard lock(m_mutex);
    const auto formatted = m_formatter->Format(event);
    for (auto& appender : m_appenders) {
        appender->Append(formatted);
    }
}

void Logger::SetLevel(LogLevel level) noexcept {
    m_level.store(level, std::memory_order_release);
}

bool Logger::IsLevelEnabled(LogLevel level) const noexcept {
    const auto current = m_level.load(std::memory_order_acquire);
    return level >= current && level != LogLevel::kOff;
}

void Logger::SetFormatter(std::unique_ptr<Formatter> formatter) {
    if (!formatter) {
        return;
    }
    std::lock_guard lock(m_mutex);
    m_formatter = std::move(formatter);
}

void Logger::AddAppender(std::unique_ptr<Appender> appender) {
    if (!appender) {
        return;
    }
    std::lock_guard lock(m_mutex);
    m_appenders.push_back(std::move(appender));
}

void Logger::ClearAppenders() {
    std::lock_guard lock(m_mutex);
    m_appenders.clear();
}

void Logger::Flush() {
    std::lock_guard lock(m_mutex);
    for (auto& appender : m_appenders) {
        appender->Flush();
    }
}

} // namespace solar_net
