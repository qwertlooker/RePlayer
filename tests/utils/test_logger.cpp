#include <filesystem>
#include <fstream>
#include <sstream>

#include <gtest/gtest.h>

#include "utils/logger.h"

namespace replayer {
namespace {

TEST(LoggerTest, CreatesLogFileAndWritesMessages) {
    const auto path = std::filesystem::temp_directory_path() / L"replayer-logger-test.log";
    std::filesystem::remove(path);

    Logger logger(path);
    logger.Info(L"hello");
    logger.Error(L"world");

    std::wifstream stream(path);
    ASSERT_TRUE(stream.is_open());
    std::wstringstream buffer;
    buffer << stream.rdbuf();
    const auto content = buffer.str();
    EXPECT_NE(content.find(L"hello"), std::wstring::npos);
    EXPECT_NE(content.find(L"world"), std::wstring::npos);
}

} // namespace
} // namespace replayer
