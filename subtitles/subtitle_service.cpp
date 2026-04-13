#include "subtitles/subtitle_service.h"

#include <algorithm>

namespace replayer {

Result<void> SubtitleService::LoadFromFile(const std::filesystem::path& path) {
    const auto extension = path.extension().wstring();
    Result<std::vector<SubtitleLine>> result = MakeError(ErrorCode::NotSupported, L"Unsupported subtitle format.");
    if (_wcsicmp(extension.c_str(), L".srt") == 0) {
        result = srt_parser_.ParseFile(path);
    } else if (_wcsicmp(extension.c_str(), L".lrc") == 0) {
        result = lrc_parser_.ParseFile(path);
    }

    if (!result.Ok()) {
        return result.Error();
    }

    lines_ = result.Value();
    return Ok();
}

Result<void> SubtitleService::SetLines(std::vector<SubtitleLine> lines) {
    lines_ = std::move(lines);
    return Ok();
}

const std::vector<SubtitleLine>& SubtitleService::GetAllLines() const noexcept {
    return lines_;
}

std::optional<SubtitleLine> SubtitleService::FindLineByTime(std::int64_t position_ms) const {
    if (lines_.empty()) {
        return std::nullopt;
    }

    auto iterator = std::lower_bound(
        lines_.begin(),
        lines_.end(),
        position_ms,
        [](const SubtitleLine& line, std::int64_t value) {
            return line.end_ms < value;
        });

    if (iterator == lines_.end()) {
        return std::nullopt;
    }

    if (iterator->start_ms <= position_ms && position_ms <= iterator->end_ms) {
        return *iterator;
    }

    return std::nullopt;
}

} // namespace replayer
