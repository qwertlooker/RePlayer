#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "state/playback_mode.h"
#include "subtitles/subtitle_line.h"

namespace replayer {

enum class PlayerVisualState {
    Stopped = 0,
    Playing,
    Paused
};

struct AppState {
    std::filesystem::path audio_path;
    std::filesystem::path subtitle_path;
    std::vector<SubtitleLine> subtitles;
    std::optional<int> current_subtitle_index;
    PlaybackMode playback_mode{PlaybackMode::Normal};
    PlayerVisualState player_state{PlayerVisualState::Stopped};
    std::int64_t position_ms{0};
    std::int64_t duration_ms{0};
    std::int64_t auto_pause_delay_ms{1000};
    bool is_recording{false};
    std::wstring status_text;
    std::unordered_map<int, std::filesystem::path> recording_by_sentence;
    std::optional<std::int64_t> auto_pause_resume_at_ms;
};

} // namespace replayer
