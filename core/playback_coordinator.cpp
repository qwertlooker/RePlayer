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

std::wstring ModeToText(PlaybackMode mode) {
    switch (mode) {
    case PlaybackMode::RepeatOne:
        return L"Repeat One";
    case PlaybackMode::AutoPause:
        return L"Auto Pause";
    case PlaybackMode::Normal:
    default:
        return L"Normal";
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
      logger_(std::move(logger)) {
    RefreshDerivedState();
}

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
    RefreshDerivedState();
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
    RefreshDerivedState();
    return Ok();
}

Result<void> PlaybackCoordinator::Play() {
    const auto result = player_->Play();
    if (result.Ok()) {
        state_.status_text = L"Playing";
        logger_->Info(L"Playback started");
    }
    UpdatePlayerState();
    RefreshDerivedState();
    return result;
}

Result<void> PlaybackCoordinator::Pause() {
    const auto result = player_->Pause();
    if (result.Ok()) {
        state_.status_text = L"Paused";
        logger_->Info(L"Playback paused");
    }
    UpdatePlayerState();
    RefreshDerivedState();
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
    RefreshDerivedState();
    return result;
}

Result<void> PlaybackCoordinator::Seek(std::int64_t position_ms) {
    const auto result = player_->Seek(position_ms);
    if (result.Ok()) {
        state_.position_ms = position_ms;
        UpdateCurrentSubtitle();
        logger_->Info(L"Seek to " + std::to_wstring(position_ms) + L" ms");
    }
    RefreshDerivedState();
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
    logger_->Info(L"Selected subtitle: " + std::to_wstring(subtitle_index));
    const auto seek_result = Seek(iterator->start_ms);
    RefreshDerivedState();
    return seek_result;
}

Result<void> PlaybackCoordinator::SelectPreviousSubtitle() {
    if (state_.subtitles.empty()) {
        return MakeError(ErrorCode::InvalidState, L"No subtitles loaded.");
    }

    if (!state_.current_subtitle_index.has_value()) {
        const auto first_index = state_.subtitles.front().index;
        return SelectSubtitle(first_index);
    }

    const auto iterator = std::find_if(
        state_.subtitles.begin(),
        state_.subtitles.end(),
        [this](const SubtitleLine& line) { return line.index == *state_.current_subtitle_index; });
    if (iterator == state_.subtitles.end()) {
        return MakeError(ErrorCode::InvalidState, L"Current subtitle not found.");
    }

    if (iterator == state_.subtitles.begin()) {
        return Ok();
    }

    const auto previous = std::prev(iterator);
    logger_->Info(L"Navigating to previous subtitle");
    return SelectSubtitle(previous->index);
}

Result<void> PlaybackCoordinator::SelectNextSubtitle() {
    if (state_.subtitles.empty()) {
        return MakeError(ErrorCode::InvalidState, L"No subtitles loaded.");
    }

    if (!state_.current_subtitle_index.has_value()) {
        const auto first_index = state_.subtitles.front().index;
        return SelectSubtitle(first_index);
    }

    const auto iterator = std::find_if(
        state_.subtitles.begin(),
        state_.subtitles.end(),
        [this](const SubtitleLine& line) { return line.index == *state_.current_subtitle_index; });
    if (iterator == state_.subtitles.end()) {
        return MakeError(ErrorCode::InvalidState, L"Current subtitle not found.");
    }

    const auto next = std::next(iterator);
    if (next == state_.subtitles.end()) {
        return Ok();
    }

    logger_->Info(L"Navigating to next subtitle");
    return SelectSubtitle(next->index);
}

void PlaybackCoordinator::Tick() {
    state_.position_ms = player_->GetPosition();
    state_.duration_ms = player_->GetDuration();
    UpdatePlayerState();
    UpdateCurrentSubtitle();
    HandleLearningModes();
    RefreshDerivedState();
}

Result<void> PlaybackCoordinator::SetPlaybackMode(PlaybackMode mode) {
    state_.playback_mode = mode;
    state_.status_text = L"Playback mode updated";
    logger_->Info(L"Switched playback mode to " + ModeToText(mode));
    RefreshDerivedState();
    return Ok();
}

Result<void> PlaybackCoordinator::SetAutoPauseDelayMs(std::int64_t delay_ms) {
    state_.auto_pause_delay_ms = std::max<std::int64_t>(0, delay_ms);
    logger_->Info(L"Updated auto pause delay to " + std::to_wstring(delay_ms) + L" ms");
    RefreshDerivedState();
    return Ok();
}

Result<void> PlaybackCoordinator::StartRecording() {
    if (!state_.current_subtitle_index.has_value()) {
        return MakeError(ErrorCode::InvalidState, L"Select a subtitle sentence before recording.");
    }

    if (state_.player_state == PlayerVisualState::Playing) {
        const auto pause_result = Pause();
        if (!pause_result.Ok()) {
            logger_->Error(L"Failed to pause before recording");
            return pause_result;
        }
    }

    const auto sentence_id = *state_.current_subtitle_index;
    const auto output_path = std::filesystem::path(L"recordings") / (std::to_wstring(sentence_id) + L".wav");

    const auto existing = state_.recording_by_sentence.find(sentence_id);
    if (existing != state_.recording_by_sentence.end()) {
        logger_->Info(L"Overwriting existing recording for sentence " + std::to_wstring(sentence_id));
        state_.hint_text = L"Overwritten previous recording for this sentence.";
    }

    const auto result = recorder_->Start(sentence_id, output_path);
    if (!result.Ok()) {
        logger_->Error(L"Recording start failed: " + result.Error().message);
        return result;
    }

    state_.is_recording = true;
    state_.recording_by_sentence[sentence_id] = output_path;
    state_.status_text = L"Recording";
    logger_->Info(L"Recording started for sentence " + std::to_wstring(sentence_id));
    RefreshDerivedState();
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
    RefreshDerivedState();
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

    logger_->Info(L"Playing original segment for sentence " + std::to_wstring(*state_.current_subtitle_index));
    state_.hint_text = L"Playing original audio segment.";
    const auto result = player_->PlaySegment(iterator->start_ms, iterator->end_ms);
    RefreshDerivedState();
    return result;
}

Result<void> PlaybackCoordinator::PlayRecording() {
    if (!state_.current_subtitle_index.has_value()) {
        return MakeError(ErrorCode::InvalidState, L"No subtitle sentence selected.");
    }

    const auto mapping = state_.recording_by_sentence.find(*state_.current_subtitle_index);
    if (mapping == state_.recording_by_sentence.end()) {
        return MakeError(ErrorCode::FileNotFound, L"No recording exists for the current sentence.");
    }

    logger_->Info(L"Playing recording for sentence " + std::to_wstring(*state_.current_subtitle_index));
    state_.hint_text = L"Playing your recording.";
    const auto result = recording_player_.Play(mapping->second);
    RefreshDerivedState();
    return result;
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
        if (!state_.current_subtitle_index.has_value() || *state_.current_subtitle_index != current_line->index) {
            state_.current_subtitle_index = current_line->index;
            logger_->Info(L"Current subtitle changed to " + std::to_wstring(current_line->index));
        }
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

void PlaybackCoordinator::RefreshDerivedState() {
    RefreshDisplayNames();
    RefreshCurrentSentenceState();
    RefreshActionAvailability();
    RefreshStatusTexts();
}

void PlaybackCoordinator::RefreshCurrentSentenceState() {
    if (!state_.current_subtitle_index.has_value()) {
        state_.current_sentence_text.clear();
        state_.current_sentence_start_ms = 0;
        state_.current_sentence_end_ms = 0;
        state_.has_current_sentence_recording = false;
        return;
    }

    const auto iterator = std::find_if(
        state_.subtitles.begin(),
        state_.subtitles.end(),
        [this](const SubtitleLine& line) { return line.index == *state_.current_subtitle_index; });
    if (iterator == state_.subtitles.end()) {
        state_.current_sentence_text.clear();
        state_.current_sentence_start_ms = 0;
        state_.current_sentence_end_ms = 0;
        state_.has_current_sentence_recording = false;
        return;
    }

    state_.current_sentence_text = iterator->text;
    state_.current_sentence_start_ms = iterator->start_ms;
    state_.current_sentence_end_ms = iterator->end_ms;
    state_.has_current_sentence_recording =
        state_.recording_by_sentence.find(*state_.current_subtitle_index) != state_.recording_by_sentence.end();
}

void PlaybackCoordinator::RefreshActionAvailability() {
    const bool has_audio = !state_.audio_path.empty();
    const bool has_subtitles = !state_.subtitles.empty();
    const bool has_current_sentence = state_.current_subtitle_index.has_value();

    state_.can_play = has_audio && state_.player_state != PlayerVisualState::Playing;
    state_.can_pause = has_audio && state_.player_state == PlayerVisualState::Playing;
    state_.can_stop = has_audio && state_.player_state != PlayerVisualState::Stopped;
    state_.can_seek = has_audio;

    if (has_subtitles && has_current_sentence) {
        const auto iterator = std::find_if(
            state_.subtitles.begin(),
            state_.subtitles.end(),
            [this](const SubtitleLine& line) { return line.index == *state_.current_subtitle_index; });
        if (iterator != state_.subtitles.end()) {
            state_.can_go_previous_sentence = iterator != state_.subtitles.begin();
            const auto next = std::next(iterator);
            state_.can_go_next_sentence = next != state_.subtitles.end();
        } else {
            state_.can_go_previous_sentence = false;
            state_.can_go_next_sentence = false;
        }
    } else {
        state_.can_go_previous_sentence = false;
        state_.can_go_next_sentence = false;
    }

    state_.can_play_original_segment = has_audio && has_current_sentence;
    state_.can_start_recording = has_current_sentence && !state_.is_recording;
    state_.can_stop_recording = state_.is_recording;
    state_.can_play_recording = has_current_sentence && state_.has_current_sentence_recording && !state_.is_recording;
}

void PlaybackCoordinator::RefreshStatusTexts() {
    switch (state_.player_state) {
    case PlayerVisualState::Playing:
        state_.playback_status_text = L"Playing";
        break;
    case PlayerVisualState::Paused:
        state_.playback_status_text = L"Paused";
        break;
    case PlayerVisualState::Stopped:
    default:
        state_.playback_status_text = L"Stopped";
        break;
    }

    state_.mode_status_text = ModeToText(state_.playback_mode);

    if (state_.is_recording) {
        state_.recording_status_text = L"Recording...";
    } else if (state_.has_current_sentence_recording) {
        state_.recording_status_text = L"Recording available";
    } else {
        state_.recording_status_text = L"No recording";
    }
}

void PlaybackCoordinator::RefreshDisplayNames() {
    if (!state_.audio_path.empty()) {
        state_.audio_display_name = state_.audio_path.filename().wstring();
    } else {
        state_.audio_display_name = L"No audio loaded";
    }

    if (!state_.subtitle_path.empty()) {
        state_.subtitle_display_name = state_.subtitle_path.filename().wstring();
    } else {
        state_.subtitle_display_name = L"No subtitle loaded";
    }
}

} // namespace replayer
