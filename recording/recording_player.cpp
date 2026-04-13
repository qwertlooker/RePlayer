#include "recording/recording_player.h"

#include <windows.h>
#include <mmsystem.h>

namespace replayer {

Result<void> RecordingPlayer::Play(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return MakeError(ErrorCode::FileNotFound, L"Recording file does not exist.");
    }

    if (!PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) {
        return MakeError(ErrorCode::SystemError, L"Failed to play recording.");
    }

    return Ok();
}

Result<void> RecordingPlayer::Stop() {
    PlaySoundW(nullptr, nullptr, 0);
    return Ok();
}

} // namespace replayer
