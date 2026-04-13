#pragma once

#include <filesystem>

#include "core/result.h"

namespace replayer {

class RecordingPlayer {
public:
    Result<void> Play(const std::filesystem::path& path);
    Result<void> Stop();
};

} // namespace replayer
