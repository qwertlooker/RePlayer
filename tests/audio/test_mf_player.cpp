#include <filesystem>

#include <gtest/gtest.h>

#include "audio/mf_player.h"

namespace replayer {
namespace {

constexpr wchar_t kIntegrationSamplePath[] = L"E:\\English\\02_Listening_P3P4\\audio\\article_01.mp3";

TEST(MfPlayerTest, OpenFailsForMissingFile) {
    MfPlayer player;
    const auto result = player.Open(L"tests/data/does-not-exist.mp3");
    EXPECT_FALSE(result.Ok());
    EXPECT_EQ(result.Error().code, ErrorCode::FileNotFound);
}

TEST(MfPlayerTest, OpensConfiguredIntegrationSampleAndSupportsBasicControls) {
    const auto sample_path = std::filesystem::path(kIntegrationSamplePath);
    if (!std::filesystem::exists(sample_path)) {
        GTEST_SKIP() << "Integration sample not found at configured path: " << sample_path;
    }

    MfPlayer player;
    ASSERT_TRUE(player.Open(sample_path).Ok());
    EXPECT_GT(player.GetDuration(), 0);
    EXPECT_EQ(player.GetState(), PlaybackState::Stopped);

    ASSERT_TRUE(player.Play().Ok());
    EXPECT_EQ(player.GetState(), PlaybackState::Playing);

    ASSERT_TRUE(player.Pause().Ok());
    EXPECT_EQ(player.GetState(), PlaybackState::Paused);

    const auto middle = player.GetDuration() / 2;
    ASSERT_TRUE(player.Seek(middle).Ok());
    EXPECT_GE(player.GetPosition(), 0);
    EXPECT_LE(player.GetPosition(), player.GetDuration());

    ASSERT_TRUE(player.Stop().Ok());
    EXPECT_EQ(player.GetState(), PlaybackState::Stopped);
}

} // namespace
} // namespace replayer
