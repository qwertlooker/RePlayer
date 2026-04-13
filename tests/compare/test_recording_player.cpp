#include <filesystem>

#include <gtest/gtest.h>

#include "recording/recording_player.h"

namespace replayer {
namespace {

TEST(RecordingPlayerTest, ReportsMissingFile) {
    RecordingPlayer player;
    const auto result = player.Play(L"tests/data/missing.wav");
    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.Error().code, ErrorCode::FileNotFound);
}

} // namespace
} // namespace replayer
