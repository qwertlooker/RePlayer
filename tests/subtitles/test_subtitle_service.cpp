#include <vector>

#include <gtest/gtest.h>

#include "subtitles/subtitle_service.h"

namespace replayer {
namespace {

TEST(SubtitleServiceTest, FindsLineInsideRange) {
    SubtitleService service;
    ASSERT_TRUE(service.SetLines({
        {1, 0, 999, L"One"},
        {2, 1000, 1999, L"Two"},
        {3, 2000, 2999, L"Three"},
    }).Ok());

    const auto line = service.FindLineByTime(1500);
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(line->index, 2);
    EXPECT_EQ(line->text, L"Two");
}

TEST(SubtitleServiceTest, ReturnsNulloptOutsideRange) {
    SubtitleService service;
    ASSERT_TRUE(service.SetLines({{1, 0, 500, L"Only"}}).Ok());
    EXPECT_FALSE(service.FindLineByTime(1000).has_value());
}

} // namespace
} // namespace replayer
