#include "core/playback_coordinator.h"

#include <algorithm>

namespace replayer {

namespace {

PlayerVisualState ToVisualState(PlaybackState state) {
    switch (state) {
    case PlaybackState::Playing:
        return PlayerVisualState::Playing;
    case PlaybackState::Paused:
        return PlayerVisualState::Paused;
    case PlaybackState::Stopped:
    default:
        return PlayerVisualState::Stopped;
    }
}

} // namespace

PlaybackCoordinator::PlaybackCoordinator(std::unique_ptr<IAudioPlayer> player,
                                         std::unique_ptr<IRecorder> recorder,
                                         std::unique_ptr<IClock> clock,
                                         std::shared_ptr<Logger> logger)
    : player_(std::move(player)),
      recorder_(std::move(recorder)),
      clock_(std::move(clock)),
      logger_(std::move(logger)) {}

Result<void> PlaybackCoordinator::LoadAudio(const std::filesystem::path& path) {
    const auto result = player_->Open(path);
    if (!result.Ok()) {
        logger_->Error(L"File load failed: " + path.wstring() + L" - " + result.Error().message);
        return result;
    }

    state_.audio_path = path;
    state_.duration_ms = player_->GetDuration();
    state_.position_ms = 0;
    state_.player_state = ToVisualState(player_->GetState());
    state_.status_text = L"Audio loaded";
    logger_->Info(L"Loaded audio: " + path.wstring());
    return Ok();
}

Result<void> PlaybackCoordinator::LoadSubtitles(const std::filesystem::path& path) {
    const auto result = subtitle_service_.LoadFromFile(path);
    if (!result.Ok()) {
        logger_->Error(L"Subtitle parse failed: " + path.wstring() + L" - " + result.Error().message);
        return result;
    }

    state_.subtitle_path = path;
    state_.subtitles = subtitle_service_.GetAllLines();
    state_.current_subtitle_index.reset();
    state_.status_text = L"Subtitles loaded";
    logger_->Info(L"Loaded subtitles: " + path.wstring());
    return Ok();
}

Result<void> PlaybackCoordinator::Play() {
    const auto result = player_->Play();
    if (result.Ok()) {
        state_.status_text = L"Playing";
        logger_->Info(L"Playback started");
    }
    UpdatePlayerState();
    return result;
}

Result<void> PlaybackCoordinator::Pause() {
    const auto result = player_->Pause();
    if (result.Ok()) {
        state_.status_text = L"Paused";
        logger_->Info(L"Playback paused");
    }
    UpdatePlayerState();
    return result;
}

Result<void> PlaybackCoordinator::Stop() {
    const auto result = player_->Stop();
    if (result.Ok()) {
        state_.position_ms = 0;
        state_.status_text = L"Stopped";
        logger_->Info(L"Playback stopped");
    }
    UpdatePlayerState();
    UpdateCurrentSubtitle();
    return result;
}

Result<void> PlaybackCoordinator::Seek(std::int64_t position_ms) {
    const auto result = player_->Seek(position_ms);
    if (result.Ok()) {
        state_.position_ms = position_ms;
        UpdateCurrentSubtitle();
        logger_->Info(L"Seek to " + std::to_wstring(position_ms) + L" ms");
    }
    return result;
}

Result<void> PlaybackCoordinator::SelectSubtitle(int subtitle_index) {
    const auto iterator = std::find_if(
        state_.subtitles.begin(),
        state_.subtitles.end(),
        [subtitle_index](const SubtitleLine& line) { return line.index == subtitle_index; });
    if (iterator == state_.subtitles.end()) {
        return MakeError(ErrorCode::InvalidArgument, L"Subtitle index not found.");
    }

    state_.current_subtitle_index = subtitle_index;
    return Seek(iterator->start_ms);
}

void PlaybackCoordinator::Tick() {
    state_.position_ms = player_->GetPosition();
    state_.duration_ms = player_->GetDuration();
    UpdatePlayerState();
    UpdateCurrentSubtitle();
    HandleLearningModes();
}

Result<void> PlaybackCoordinator::SetPlaybackMode(PlaybackMode mode) {
    state_.playback_mode = mode;
    state_.status_text = L"Playback mode updated";
    logger_->Info(L"Switched playback mode");
    return Ok();
}

Result<void> PlaybackCoordinator::SetAutoPauseDelayMs(std::int64_t delay_ms) {
    state_.auto_pause_delay_ms = std::max<std::int64_t>(0, delay_ms);
    logger_->Info(L"Updated auto pause delay");
    return Ok();
}

Result<void> PlaybackCoordinator::StartRecording() {
    if (!state_.current_subtitle_index.has_value()) {
        return MakeError(ErrorCode::InvalidState, L"Select a subtitle sentence before recording.");
    }

    const auto sentence_id = *state_.current_subtitle_index;
    const auto output_path = std::filesystem::path(L"recordings") / (std::to_wstring(sentence_id) + L".wav");
    const auto result = recorder_->Start(sentence_id, output_path);
    if (!result.Ok()) {
        logger_->Error(L"Recording start failed: " + result.Error().message);
        return result;
    }

    state_.is_recording = true;
    state_.recording_by_sentence[sentence_id] = output_path;
    state_.status_text = L"Recording";
    logger_->Info(L"Recording started");
    return Ok();
}

Result<void> PlaybackCoordinator::StopRecording() {
    const auto result = recorder_->Stop();
    if (!result.Ok()) {
        logger_->Error(L"Recording stop failed: " + result.Error().message);
        return result;
    }
    state_.is_recording = false;
    state_.status_text = L"Recording stopped";
    logger_->Info(L"Recording stopped");
    return Ok();
}

Result<void> PlaybackCoordinator::PlayOriginalSegment() {
    if (!state_.current_subtitle_index.has_value()) {
        return MakeError(ErrorCode::InvalidState, L"No subtitle sentence selected.");
    }

    const auto iterator = std::find_if(
        state_.subtitles.begin(),
        state_.subtitles.end(),
        [this](const SubtitleLine& line) { return line.index == *state_.current_subtitle_index; });
    if (iterator == state_.subtitles.end()) {
        return MakeError(ErrorCode::InvalidState, L"Current subtitle sentence not found.");
    }

    return player_->PlaySegment(iterator->start_ms, iterator->end_ms);
}

Result<void> PlaybackCoordinator::PlayRecording() {
    if (!state_.current_subtitle_index.has_value()) {
        return MakeError(ErrorCode::InvalidState, L"No subtitle sentence selected.");
    }

    const auto mapping = state_.recording_by_sentence.find(*state_.current_subtitle_index);
    if (mapping == state_.recording_by_sentence.end()) {
        return MakeError(ErrorCode::FileNotFound, L"No recording exists for the current sentence.");
    }

    return recording_player_.Play(mapping->second);
}

const AppState& PlaybackCoordinator::GetState() const noexcept {
    return state_;
}

SubtitleService& PlaybackCoordinator::SubtitleServiceRef() noexcept {
    return subtitle_service_;
}

IAudioPlayer& PlaybackCoordinator::PlayerRef() noexcept {
    return *player_;
}

void PlaybackCoordinator::UpdateCurrentSubtitle() {
    const auto current_line = subtitle_service_.FindLineByTime(state_.position_ms);
    if (current_line.has_value()) {
        state_.current_subtitle_index = current_line->index;
    } else {
        state_.current_subtitle_index.reset();
    }
}

void PlaybackCoordinator::UpdatePlayerState() {
    state_.player_state = ToVisualState(player_->GetState());
}

void PlaybackCoordinator::HandleLearningModes() {
    if (!state_.current_subtitle_index.has_value()) {
        if (state_.player_state == PlayerVisualState::Stopped) {
            state_.auto_pause_resume_at_ms.reset();
        }
        return;
    }

    const auto iterator = std::find_if(
        state_.subtitles.begin(),
        state_.subtitles.end(),
        [this](const SubtitleLine& line) { return line.index == *state_.current_subtitle_index; });
    if (iterator == state_.subtitles.end()) {
        return;
    }

    if (state_.playback_mode == PlaybackMode::RepeatOne) {
        if (state_.position_ms >= iterator->end_ms && state_.player_state == PlayerVisualState::Playing) {
            Seek(iterator->start_ms);
            Play();
        }
        return;
    }

    if (state_.playback_mode == PlaybackMode::AutoPause) {
        const auto now_ms = clock_->NowMs();
        if (state_.position_ms >= iterator->end_ms && state_.player_state == PlayerVisualState::Playing) {
            Pause();
            state_.auto_pause_resume_at_ms = now_ms + state_.auto_pause_delay_ms;
            return;
        }

        if (state_.auto_pause_resume_at_ms.has_value() && now_ms >= *state_.auto_pause_resume_at_ms) {
            state_.auto_pause_resume_at_ms.reset();
            auto next_iterator = iterator;
            ++next_iterator;
            if (next_iterator != state_.subtitles.end()) {
                Seek(next_iterator->start_ms);
            }
            Play();
        }
    }
}

} // namespace replayer
