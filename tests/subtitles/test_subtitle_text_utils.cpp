#include <gtest/gtest.h>

#include "subtitles/subtitle_text_utils.h"

namespace replayer {
namespace {

TEST(SubtitleTextUtilsTest, RemovesCommonSrtFormattingTags) {
    const std::wstring input = L"Hello <i>world</i>!\n<font color=\"red\">Line 2</font>";
    const std::wstring output = PrepareSubtitleTextForDisplay(input);
    EXPECT_EQ(output, L"Hello world!\nLine 2");
}

TEST(SubtitleTextUtilsTest, KeepsTextWhenTagIsNotClosed) {
    const std::wstring input = L"Hello <unfinished";
    const std::wstring output = PrepareSubtitleTextForDisplay(input);
    EXPECT_EQ(output, L"Hello <unfinished");
}

TEST(SubtitleTextUtilsTest, PreservesMultilineAfterRemovingTagsAndCarriageReturn) {
    const std::wstring input = L"<i>Line 1</i>\r\nLine <b>2</b>";
    const std::wstring output = PrepareSubtitleTextForDisplay(input);
    EXPECT_EQ(output, L"Line 1\nLine 2");
}

} // namespace
} // namespace replayer
