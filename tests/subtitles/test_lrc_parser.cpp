#include <gtest/gtest.h>

#include "subtitles/lrc_parser.h"

namespace replayer {
namespace {

TEST(LrcParserTest, ParsesMultiTimestampLine) {
    LrcParser parser;
    const auto result = parser.ParseText(L"[00:01.00][00:02.50]Hello\n[00:04.00]World\n");

    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(result.Value().size(), 3U);
    EXPECT_EQ(result.Value()[0].start_ms, 1000);
    EXPECT_EQ(result.Value()[1].start_ms, 2500);
    EXPECT_EQ(result.Value()[0].text, L"Hello");
}

TEST(LrcParserTest, IgnoresLinesWithoutTimestamps) {
    LrcParser parser;
    const auto result = parser.ParseText(L"metadata\n[00:01.00]Line\n");

    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(result.Value().size(), 1U);
    EXPECT_EQ(result.Value()[0].text, L"Line");
}

} // namespace
} // namespace replayer
