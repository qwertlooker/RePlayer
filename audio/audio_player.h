#pragma once

#include <cstdint>
#include <filesystem>

#include "core/result.h"

namespace replayer {

enum class PlaybackState {
    Stopped = 0,
    Playing,
    Paused
};

class IAudioPlayer {
public:
    virtual ~IAudioPlayer() = default;

    virtual Result<void> Open(const std::filesystem::path& path) = 0;
    virtual Result<void> Play() = 0;
    virtual Result<void> Pause() = 0;
    virtual Result<void> Stop() = 0;
    virtual Result<void> Seek(std::int64_t position_ms) = 0;
    virtual Result<void> PlaySegment(std::int64_t start_ms, std::int64_t end_ms) = 0;
    virtual std::int64_t GetPosition() const = 0;
    virtual std::int64_t GetDuration() const = 0;
    virtual PlaybackState GetState() const = 0;
};

} // namespace replayer
