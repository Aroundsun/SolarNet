#include "solar_net/base/logger.h"

#include <format>
#include <iostream>

int main() {
    solar_net::Logger::GetInstance().SetLevel(solar_net::LogLevel::kDebug);

    LOG_DEBUG("debug message");
    LOG_INFO("info message");
    LOG_WARN("warn message");
    LOG_ERROR("error message");

    LOG_INFO(std::format("formatted: {} {}", 42, "hello"));

    return 0;
}
