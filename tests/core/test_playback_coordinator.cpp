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

TEST_F(PlaybackCoordinatorTest, LoadAudioSetsDisplayName) {
    ASSERT_TRUE(coordinator_->LoadAudio(L"test.mp3").Ok());
    EXPECT_EQ(coordinator_->GetState().audio_display_name, L"test.mp3");
}

TEST_F(PlaybackCoordinatorTest, LoadAudioSetsCanPlay) {
    ASSERT_TRUE(coordinator_->LoadAudio(L"test.mp3").Ok());
    EXPECT_TRUE(coordinator_->GetState().can_play);
    EXPECT_FALSE(coordinator_->GetState().can_pause);
    EXPECT_FALSE(coordinator_->GetState().can_stop);
}

TEST_F(PlaybackCoordinatorTest, LoadSubtitlesSetsDisplayName) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({{1, 0, 1000, L"Test"}}).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.subtitle_path = L"test.srt";
    coordinator_->SetPlaybackMode(PlaybackMode::Normal);
    EXPECT_FALSE(coordinator_->GetState().subtitle_display_name.empty());
}

TEST_F(PlaybackCoordinatorTest, LoadSubtitlesSetsCurrentSentenceState) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({{1, 100, 500, L"Hello"}}).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 1;
    coordinator_->SetPlaybackMode(PlaybackMode::Normal);

    EXPECT_EQ(coordinator_->GetState().current_sentence_text, L"Hello");
    EXPECT_EQ(coordinator_->GetState().current_sentence_start_ms, 100);
    EXPECT_EQ(coordinator_->GetState().current_sentence_end_ms, 500);
}

TEST_F(PlaybackCoordinatorTest, SelectPreviousSubtitleSeeksCorrectly) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({
        {1, 0, 999, L"First"},
        {2, 1000, 1999, L"Second"},
        {3, 2000, 2999, L"Third"},
    }).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 3;

    ASSERT_TRUE(coordinator_->SelectPreviousSubtitle().Ok());
    EXPECT_EQ(state.current_subtitle_index, 2);
    EXPECT_FALSE(player_->seek_history.empty());
    EXPECT_EQ(player_->seek_history.back(), 1000);
}

TEST_F(PlaybackCoordinatorTest, SelectNextSubtitleSeeksCorrectly) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({
        {1, 0, 999, L"First"},
        {2, 1000, 1999, L"Second"},
        {3, 2000, 2999, L"Third"},
    }).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 1;

    ASSERT_TRUE(coordinator_->SelectNextSubtitle().Ok());
    EXPECT_EQ(state.current_subtitle_index, 2);
    EXPECT_FALSE(player_->seek_history.empty());
    EXPECT_EQ(player_->seek_history.back(), 1000);
}

TEST_F(PlaybackCoordinatorTest, PreviousSubtitleStopsAtFirst) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({
        {1, 0, 999, L"First"},
        {2, 1000, 1999, L"Second"},
    }).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 1;

    ASSERT_TRUE(coordinator_->SelectPreviousSubtitle().Ok());
    EXPECT_EQ(state.current_subtitle_index, 1);
}

TEST_F(PlaybackCoordinatorTest, NextSubtitleStopsAtLast) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({
        {1, 0, 999, L"First"},
        {2, 1000, 1999, L"Second"},
    }).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 2;

    ASSERT_TRUE(coordinator_->SelectNextSubtitle().Ok());
    EXPECT_EQ(state.current_subtitle_index, 2);
}

TEST_F(PlaybackCoordinatorTest, RecordingPausesPlaybackFirst) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({{1, 0, 1000, L"Test"}}).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 1;

    ASSERT_TRUE(coordinator_->LoadAudio(L"test.mp3").Ok());
    ASSERT_TRUE(coordinator_->Play().Ok());
    EXPECT_EQ(player_->pause_calls, 0);

    ASSERT_TRUE(coordinator_->StartRecording().Ok());
    EXPECT_GE(player_->pause_calls, 1);
}

TEST_F(PlaybackCoordinatorTest, HasCurrentSentenceRecordingAfterRecording) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({{1, 0, 1000, L"Test"}}).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 1;

    EXPECT_FALSE(coordinator_->GetState().has_current_sentence_recording);

    ASSERT_TRUE(coordinator_->StartRecording().Ok());
    ASSERT_TRUE(coordinator_->StopRecording().Ok());

    EXPECT_TRUE(coordinator_->GetState().has_current_sentence_recording);
}

TEST_F(PlaybackCoordinatorTest, NoCurrentSentenceDisablesActions) {
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.current_subtitle_index.reset();

    EXPECT_FALSE(coordinator_->GetState().can_play_original_segment);
    EXPECT_FALSE(coordinator_->GetState().can_start_recording);
    EXPECT_FALSE(coordinator_->GetState().can_play_recording);
    EXPECT_FALSE(coordinator_->GetState().can_go_previous_sentence);
    EXPECT_FALSE(coordinator_->GetState().can_go_next_sentence);
}

TEST_F(PlaybackCoordinatorTest, PlaybackStatusTextCorrect) {
    EXPECT_EQ(coordinator_->GetState().playback_status_text, L"Stopped");

    ASSERT_TRUE(coordinator_->LoadAudio(L"test.mp3").Ok());
    ASSERT_TRUE(coordinator_->Play().Ok());
    EXPECT_EQ(coordinator_->GetState().playback_status_text, L"Playing");

    ASSERT_TRUE(coordinator_->Pause().Ok());
    EXPECT_EQ(coordinator_->GetState().playback_status_text, L"Paused");

    ASSERT_TRUE(coordinator_->Stop().Ok());
    EXPECT_EQ(coordinator_->GetState().playback_status_text, L"Stopped");
}

TEST_F(PlaybackCoordinatorTest, ModeStatusTextCorrect) {
    coordinator_->SetPlaybackMode(PlaybackMode::Normal);
    EXPECT_EQ(coordinator_->GetState().mode_status_text, L"Normal");

    coordinator_->SetPlaybackMode(PlaybackMode::RepeatOne);
    EXPECT_EQ(coordinator_->GetState().mode_status_text, L"Repeat One");

    coordinator_->SetPlaybackMode(PlaybackMode::AutoPause);
    EXPECT_EQ(coordinator_->GetState().mode_status_text, L"Auto Pause");
}

TEST_F(PlaybackCoordinatorTest, RecordingStatusTextCorrect) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({{1, 0, 1000, L"Test"}}).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 1;

    EXPECT_EQ(coordinator_->GetState().recording_status_text, L"No recording");

    ASSERT_TRUE(coordinator_->StartRecording().Ok());
    EXPECT_EQ(coordinator_->GetState().recording_status_text, L"Recording...");

    ASSERT_TRUE(coordinator_->StopRecording().Ok());
    EXPECT_EQ(coordinator_->GetState().recording_status_text, L"Recording available");
}

TEST_F(PlaybackCoordinatorTest, CanPlayRecordingOnlyWhenRecordingExists) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({{1, 0, 1000, L"Test"}}).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index = 1;

    EXPECT_FALSE(coordinator_->GetState().can_play_recording);

    ASSERT_TRUE(coordinator_->StartRecording().Ok());
    ASSERT_TRUE(coordinator_->StopRecording().Ok());

    EXPECT_TRUE(coordinator_->GetState().can_play_recording);
}

TEST_F(PlaybackCoordinatorTest, CanGoPreviousNextCorrect) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({
        {1, 0, 999, L"First"},
        {2, 1000, 1999, L"Second"},
        {3, 2000, 2999, L"Third"},
    }).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();

    state.current_subtitle_index = 1;
    coordinator_->SetPlaybackMode(PlaybackMode::Normal);
    EXPECT_FALSE(coordinator_->GetState().can_go_previous_sentence);
    EXPECT_TRUE(coordinator_->GetState().can_go_next_sentence);

    state.current_subtitle_index = 2;
    coordinator_->SetPlaybackMode(PlaybackMode::Normal);
    EXPECT_TRUE(coordinator_->GetState().can_go_previous_sentence);
    EXPECT_TRUE(coordinator_->GetState().can_go_next_sentence);

    state.current_subtitle_index = 3;
    coordinator_->SetPlaybackMode(PlaybackMode::Normal);
    EXPECT_TRUE(coordinator_->GetState().can_go_previous_sentence);
    EXPECT_FALSE(coordinator_->GetState().can_go_next_sentence);
}

TEST_F(PlaybackCoordinatorTest, SelectPreviousWithoutCurrentGoesToFirst) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({
        {1, 0, 999, L"First"},
        {2, 1000, 1999, L"Second"},
    }).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index.reset();

    ASSERT_TRUE(coordinator_->SelectPreviousSubtitle().Ok());
    EXPECT_EQ(state.current_subtitle_index, 1);
}

TEST_F(PlaybackCoordinatorTest, SelectNextWithoutCurrentGoesToFirst) {
    ASSERT_TRUE(coordinator_->SubtitleServiceRef().SetLines({
        {1, 0, 999, L"First"},
        {2, 1000, 1999, L"Second"},
    }).Ok());
    auto& state = const_cast<AppState&>(coordinator_->GetState());
    state.subtitles = coordinator_->SubtitleServiceRef().GetAllLines();
    state.current_subtitle_index.reset();

    ASSERT_TRUE(coordinator_->SelectNextSubtitle().Ok());
    EXPECT_EQ(state.current_subtitle_index, 1);
}

TEST_F(PlaybackCoordinatorTest, NoSubtitlesReturnsErrorForNavigation) {
    auto result = coordinator_->SelectPreviousSubtitle();
    EXPECT_FALSE(result.Ok());

    result = coordinator_->SelectNextSubtitle();
    EXPECT_FALSE(result.Ok());
}

} // namespace
} // namespace replayer
