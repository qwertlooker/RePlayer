#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "core/result.h"
#include "subtitles/lrc_parser.h"
#include "subtitles/srt_parser.h"
#include "subtitles/subtitle_line.h"

namespace replayer {

class SubtitleService {
public:
    Result<void> LoadFromFile(const std::filesystem::path& path);
    Result<void> SetLines(std::vector<SubtitleLine> lines);
    [[nodiscard]] const std::vector<SubtitleLine>& GetAllLines() const noexcept;
    [[nodiscard]] std::optional<SubtitleLine> FindLineByTime(std::int64_t position_ms) const;

private:
    std::vector<SubtitleLine> lines_;
    SrtParser srt_parser_;
    LrcParser lrc_parser_;
};

} // namespace replayer
