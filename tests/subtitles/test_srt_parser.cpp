#include <gtest/gtest.h>

#include "subtitles/srt_parser.h"

namespace replayer {
namespace {

TEST(SrtParserTest, ParsesValidContent) {
    SrtParser parser;
    const auto result = parser.ParseText(
        L"1\n00:00:01,000 --> 00:00:02,500\nHello world\n\n"
        L"2\n00:00:03,000 --> 00:00:04,000\nSecond line\n");

    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(result.Value().size(), 2U);
    EXPECT_EQ(result.Value()[0].start_ms, 1000);
    EXPECT_EQ(result.Value()[0].end_ms, 2500);
    EXPECT_EQ(result.Value()[0].text, L"Hello world");
}

TEST(SrtParserTest, RejectsMalformedRange) {
    SrtParser parser;
    const auto result = parser.ParseText(L"1\nbad range\nHello\n");
    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.Error().code, ErrorCode::ParseError);
}

TEST(SrtParserTest, PreservesMultilineBlocks) {
    SrtParser parser;
    const auto result = parser.ParseText(
        L"1\n00:00:01,000 --> 00:00:03,000\nFirst line\nSecond line\n\n");

    ASSERT_TRUE(result.Ok());
    ASSERT_EQ(result.Value().size(), 1U);
    EXPECT_EQ(result.Value()[0].text, L"First line\nSecond line");
}

} // namespace
} // namespace replayer
