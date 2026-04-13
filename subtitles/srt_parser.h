#pragma once

#include <filesystem>
#include <vector>

#include "core/result.h"
#include "subtitles/subtitle_line.h"

namespace replayer {

class SrtParser {
public:
    Result<std::vector<SubtitleLine>> ParseFile(const std::filesystem::path& path) const;
    Result<std::vector<SubtitleLine>> ParseText(const std::wstring& text) const;
};

} // namespace replayer
