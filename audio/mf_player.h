#pragma once

#include <string>

#include "audio/audio_player.h"

namespace replayer {

class MfPlayer final : public IAudioPlayer {
public:
    MfPlayer();
    ~MfPlayer() override;

    Result<void> Open(const std::filesystem::path& path) override;
    Result<void> Play() override;
    Result<void> Pause() override;
    Result<void> Stop() override;
    Result<void> Seek(std::int64_t position_ms) override;
    Result<void> PlaySegment(std::int64_t start_ms, std::int64_t end_ms) override;
    std::int64_t GetPosition() const override;
    std::int64_t GetDuration() const override;
    PlaybackState GetState() const override;

private:
    Result<void> Execute(const std::wstring& command, bool expect_result = false, std::wstring* result = nullptr) const;
    Result<void> CloseAlias();
    Result<void> RefreshPosition() const;

    std::wstring alias_;
    std::filesystem::path path_;
    mutable std::int64_t cached_position_ms_{0};
    std::int64_t duration_ms_{0};
    PlaybackState state_{PlaybackState::Stopped};
    bool opened_{false};
};

} // namespace replayer
