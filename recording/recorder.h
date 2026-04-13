#pragma once

#include <filesystem>

#include "core/result.h"

namespace replayer {

class IRecorder {
public:
    virtual ~IRecorder() = default;
    virtual Result<void> Start(int sentence_id, const std::filesystem::path& output_path) = 0;
    virtual Result<void> Stop() = 0;
    virtual bool IsRecording() const = 0;
};

} // namespace replayer
