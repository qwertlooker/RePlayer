#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include "audio/audio_player.h"
#include "core/clock.h"
#include "recording/recorder.h"
#include "recording/recording_player.h"
#include "state/app_state.h"
#include "subtitles/subtitle_service.h"
#include "utils/logger.h"

namespace replayer {

class PlaybackCoordinator {
public:
    PlaybackCoordinator(std::unique_ptr<IAudioPlayer> player,
                        std::unique_ptr<IRecorder> recorder,
                        std::unique_ptr<IClock> clock,
                        std::shared_ptr<Logger> logger);

    Result<void> LoadAudio(const std::filesystem::path& path);
    Result<void> LoadSubtitles(const std::filesystem::path& path);
    Result<void> Play();
    Result<void> Pause();
    Result<void> Stop();
    Result<void> Seek(std::int64_t position_ms);
    Result<void> SelectSubtitle(int subtitle_index);
    Result<void> SelectPreviousSubtitle();
    Result<void> SelectNextSubtitle();
    void Tick();

    Result<void> SetPlaybackMode(PlaybackMode mode);
    Result<void> SetAutoPauseDelayMs(std::int64_t delay_ms);
    Result<void> StartRecording();
    Result<void> StopRecording();
    Result<void> PlayOriginalSegment();
    Result<void> PlayRecording();

    [[nodiscard]] const AppState& GetState() const noexcept;
    [[nodiscard]] SubtitleService& SubtitleServiceRef() noexcept;
    [[nodiscard]] IAudioPlayer& PlayerRef() noexcept;

private:
    enum class AutoSubtitleLoadResult {
        None = 0,
        Success,
        NotFound,
        Failed
    };

    void UpdateCurrentSubtitle();
    void UpdatePlayerState();
    void HandleLearningModes();
    void RefreshDerivedState();
    void RefreshCurrentSentenceState();
    void RefreshActionAvailability();
    void RefreshStatusTexts();
    void RefreshDisplayNames();

    std::unique_ptr<IAudioPlayer> player_;
    std::unique_ptr<IRecorder> recorder_;
    std::unique_ptr<IClock> clock_;
    std::shared_ptr<Logger> logger_;
    SubtitleService subtitle_service_;
    RecordingPlayer recording_player_;
    AppState state_;
    AutoSubtitleLoadResult auto_subtitle_load_result_{AutoSubtitleLoadResult::None};
};

} // namespace replayer
