#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "log.h"

using solar_net::log::Level;
using solar_net::log::Options;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in),
                        std::istreambuf_iterator<char>());
}

void remove_file(const std::string& path) {
    std::remove(path.c_str());
}

} // namespace

class LogTest : public ::testing::Test {
protected:
    void TearDown() override {
        solar_net::log::shutdown();
        remove_file("test_log_output.log");
    }
};

TEST_F(LogTest, InitAndShutdown) {
    EXPECT_FALSE(solar_net::log::is_initialized());

    Options opts;
    opts.console = true;
    opts.level = Level::Info;
    EXPECT_TRUE(solar_net::log::init(opts));
    EXPECT_TRUE(solar_net::log::is_initialized());
    EXPECT_NE(solar_net::log::logger(), nullptr);

    solar_net::log::shutdown();
    EXPECT_FALSE(solar_net::log::is_initialized());
}

TEST_F(LogTest, LevelFilter) {
    Options opts;
    opts.console = false;
    opts.file_path = "test_log_output.log";
    opts.level = Level::Warn;
    ASSERT_TRUE(solar_net::log::init(opts));

    SNLOG_INFO("hidden");
    SNLOG_WARN("visible");

    solar_net::log::shutdown();

    const std::string content = read_file("test_log_output.log");
    EXPECT_EQ(content.find("hidden"), std::string::npos);
    EXPECT_NE(content.find("visible"), std::string::npos);
}

TEST_F(LogTest, SetLevelAtRuntime) {
    Options opts;
    opts.console = false;
    opts.file_path = "test_log_output.log";
    opts.level = Level::Err;
    ASSERT_TRUE(solar_net::log::init(opts));

    solar_net::log::set_level(Level::Info);
    EXPECT_EQ(solar_net::log::level(), Level::Info);

    SNLOG_INFO("after level change");

    solar_net::log::shutdown();

    const std::string content = read_file("test_log_output.log");
    EXPECT_NE(content.find("after level change"), std::string::npos);
}

TEST_F(LogTest, MacrosNoOpWhenNotInitialized) {
    SNLOG_INFO("should not crash");
    EXPECT_FALSE(solar_net::log::is_initialized());
}

TEST_F(LogTest, InitFailsWithoutSinks) {
    Options opts;
    opts.console = false;
    opts.file_path.clear();
    EXPECT_FALSE(solar_net::log::init(opts));
    EXPECT_FALSE(solar_net::log::is_initialized());
}
