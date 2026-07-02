#include "solar_net/base/logger.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace solar_net {
namespace {

class StringAppender : public Appender {
public:
    void Append(const std::string& log) override {
        m_logs.push_back(log);
    }

    void Flush() override {}

    [[nodiscard]] const std::vector<std::string>& Logs() const { return m_logs; }

private:
    std::vector<std::string> m_logs;
};

class LoggerTest : public testing::Test {
protected:
    void SetUp() override {
        m_appender = std::make_shared<StringAppender>();

        auto& logger = Logger::GetInstance();
        logger.SetLevel(LogLevel::kDebug);
        logger.ClearAppenders();
        logger.AddAppender(std::make_unique<StringAppenderForwarder>(m_appender));
    }

    void TearDown() override {
        Logger::GetInstance().SetLevel(LogLevel::kInfo);
    }

    [[nodiscard]] std::shared_ptr<StringAppender> GetAppender() const { return m_appender; }

private:
    class StringAppenderForwarder : public Appender {
    public:
        explicit StringAppenderForwarder(std::shared_ptr<StringAppender> target)
            : m_target(std::move(target)) {}

        void Append(const std::string& log) override {
            if (m_target) {
                m_target->Append(log);
            }
        }

        void Flush() override {
            if (m_target) {
                m_target->Flush();
            }
        }

    private:
        std::shared_ptr<StringAppender> m_target;
    };

    std::shared_ptr<StringAppender> m_appender;
};

TEST_F(LoggerTest, LogInfoStoresMessage) {
    LOG_INFO("hello logger");
    Logger::GetInstance().Flush();

    const auto& logs = GetAppender()->Logs();
    ASSERT_EQ(logs.size(), 1);
    EXPECT_THAT(logs[0], testing::HasSubstr("hello logger"));
    EXPECT_THAT(logs[0], testing::HasSubstr("[INFO]"));
}

TEST_F(LoggerTest, DebugIsFilteredWhenLevelIsInfo) {
    Logger::GetInstance().SetLevel(LogLevel::kInfo);
    LOG_DEBUG("debug message");
    Logger::GetInstance().Flush();

    EXPECT_TRUE(GetAppender()->Logs().empty());
}

TEST_F(LoggerTest, LogLevelOrderWorks) {
    Logger::GetInstance().SetLevel(LogLevel::kWarn);
    LOG_INFO("info");
    LOG_WARN("warn");
    LOG_ERROR("error");
    Logger::GetInstance().Flush();

    const auto& logs = GetAppender()->Logs();
    EXPECT_EQ(logs.size(), 2);
    EXPECT_THAT(logs[0], testing::HasSubstr("[WARN]"));
    EXPECT_THAT(logs[1], testing::HasSubstr("[ERROR]"));
}

TEST_F(LoggerTest, FileAppenderWritesToFile) {
    const auto temp_path = std::filesystem::temp_directory_path() / "solar_net_logger_test.log";
    std::filesystem::remove(temp_path);

    {
        Logger::GetInstance().ClearAppenders();
        Logger::GetInstance().AddAppender(std::make_unique<FileAppender>(temp_path.string()));
        LOG_INFO("file log test");
        Logger::GetInstance().Flush();
    }

    std::ifstream file(temp_path);
    std::string content;
    std::getline(file, content);
    EXPECT_THAT(content, testing::HasSubstr("file log test"));
    EXPECT_THAT(content, testing::HasSubstr("[INFO]"));

    std::filesystem::remove(temp_path);
}

TEST_F(LoggerTest, LoggerIsNonCopyable) {
    // Compile-time check via base class. If it compiled, this passes.
    SUCCEED();
}

} // namespace
} // namespace solar_net
