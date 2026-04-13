#include <gtest/gtest.h>

#include "core/time_format.h"

namespace replayer {
namespace {

TEST(TimeFormatTest, FormatsMinutesAndSeconds) {
    EXPECT_EQ(FormatTimestamp(0), L"00:00");
    EXPECT_EQ(FormatTimestamp(65'000), L"01:05");
}

} // namespace
} // namespace replayer
