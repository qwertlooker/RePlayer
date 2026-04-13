#include <filesystem>
#include <memory>

#include <gtest/gtest.h>

#include "core/playback_coordinator.h"
#include "tests/test_doubles.h"
#include "utils/logger.h"

namespace replayer {
namespace {

class PlaybackCoordinatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto player = std::make_unique<tests::FakeAudioPlayer>();
        auto recorder = std::make_unique<tests::FakeRecorder>();
        auto clock = std::make_unique<tests::FakeClock>();

        player_ = player.get();
        recorder_ = recorder.get();
        clock_ = clock.get();

        const auto log_path = std::filesystem::temp_directory_path() / L"replayer-playback-test.log";
        coordinator_ = std::make_unique<PlaybackCoordinator>(
            std::move(player),
            std::move(recorder),
            std::move(clock),
            std::make_shared<Logger>(log_path));
    }

    tests::FakeAudioPlayer* player_{nullptr};
    tests::FakeRecorder* recorder_{nullptr};
    tests::FakeClock* clock_{nullptr};
    std::unique_ptr<PlaybackCoordinator> coordinator_;
};

TEST_F(PlaybackCoordinatorTest, SelectSubtitleSeeksToSentenceStart) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({
        {1, 0, 999, L"One"},
        {2, 1000, 1999, L"Two"},
    }).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();

    ASSERT_TRUE(coordinator_->SelectSubtitle(2).Ok());
    ASSERT_FALSE(player_->seek_history.empty());
    EXPECT_EQ(player_->seek_history.back(), 1000);
}

TEST_F(PlaybackCoordinatorTest, RepeatOneRewindsAtSentenceEnd) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({{1, 0, 1000, L"Line"}}).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 1;

    coordinator_->SetPlaybackMode(PlaybackMode::RepeatOne);
    player_->position_ms = 1000;
    player_->state = PlaybackState::Playing;

    coordinator_->Tick();

    ASSERT_FALSE(player_->seek_history.empty());
    EXPECT_EQ(player_->seek_history.back(), 0);
    EXPECT_GT(player_->play_calls, 0);
}

TEST_F(PlaybackCoordinatorTest, AutoPauseWaitsThenResumesNextSentence) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({
        {1, 0, 1000, L"One"},
        {2, 1500, 2500, L"Two"},
    }).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 1;

    coordinator_->SetPlaybackMode(PlaybackMode::AutoPause);
    coordinator_->SetAutoPauseDelayMs(500);
    player_->position_ms = 1000;
    player_->state = PlaybackState::Playing;
    clock_->SetNowMs(100);

    coordinator_->Tick();
    EXPECT_EQ(player_->pause_calls, 1);

    player_->state = PlaybackState::Paused;
    player_->position_ms = 1000;
    clock_->SetNowMs(700);
    coordinator_->Tick();

    EXPECT_EQ(player_->seek_history.back(), 1500);
    EXPECT_GE(player_->play_calls, 1);
}

TEST_F(PlaybackCoordinatorTest, RecordingUsesCurrentSentenceId) {
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.current_subtitle_index = 7;

    ASSERT_TRUE(coordinator_->StartRecording().Ok());
    EXPECT_TRUE(recorder_->is_recording);
    EXPECT_EQ(recorder_->started_sentence, 7);
    EXPECT_EQ(recorder_->started_path.filename(), L"7.wav");

    ASSERT_TRUE(coordinator_->StopRecording().Ok());
    EXPECT_EQ(recorder_->stop_calls, 1);
}

TEST_F(PlaybackCoordinatorTest, OriginalSegmentUsesCurrentSubtitleBounds) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({{3, 200, 600, L"Clip"}}).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 3;

    ASSERT_TRUE(coordinator_->PlayOriginalSegment().Ok());
    EXPECT_EQ(player_->segment_start, 200);
    EXPECT_EQ(player_->segment_end, 600);
}

} // namespace
} // namespace replayer
