#include "subtitles/lrc_parser.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace replayer {

namespace {

std::wstring ReadAllText(const std::filesystem::path& path) {
    std::wifstream stream(path);
    stream.imbue(std::locale(".UTF-8"));
    std::wstringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::wstring Trim(const std::wstring& value) {
    const auto first = value.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) {
        return L"";
    }

    const auto last = value.find_last_not_of(L" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::int64_t ParseLrcTimestamp(const std::wstring& token) {
    int minutes = 0;
    int seconds = 0;
    int centis = 0;
    if (swscanf_s(token.c_str(), L"%d:%d.%d", &minutes, &seconds, &centis) < 2) {
        return -1;
    }
    return (minutes * 60LL + seconds) * 1000LL + centis * 10LL;
}

} // namespace

Result<std::vector<SubtitleLine>> LrcParser::ParseFile(const std::filesystem::path& path) const {
    if (!std::filesystem::exists(path)) {
        return MakeError(ErrorCode::FileNotFound, L"LRC file not found.");
    }
    return ParseText(ReadAllText(path));
}

Result<std::vector<SubtitleLine>> LrcParser::ParseText(const std::wstring& text) const {
    static const std::wregex timestamp_pattern(LR"(\[(\d+:\d+(?:\.\d{1,3})?)\])");

    std::vector<SubtitleLine> lines;
    std::wistringstream stream(text);
    std::wstring row;
    int index = 1;

    while (std::getline(stream, row)) {
        std::wsregex_iterator iterator(row.begin(), row.end(), timestamp_pattern);
        const std::wsregex_iterator end;
        if (iterator == end) {
            continue;
        }

        std::wstring content = std::regex_replace(row, timestamp_pattern, L"");
        content = Trim(content);
        std::vector<std::int64_t> timestamps;

        for (; iterator != end; ++iterator) {
            const auto stamp = ParseLrcTimestamp((*iterator)[1].str());
            if (stamp >= 0) {
                timestamps.push_back(stamp);
            }
        }

        std::sort(timestamps.begin(), timestamps.end());
        for (const auto stamp : timestamps) {
            lines.push_back(SubtitleLine{index++, stamp, stamp + 2000, content});
        }
    }

    std::sort(lines.begin(), lines.end(), [](const SubtitleLine& left, const SubtitleLine& right) {
        return left.start_ms < right.start_ms;
    });

    for (std::size_t i = 0; i < lines.size(); ++i) {
        lines[i].index = static_cast<int>(i + 1);
        if (i + 1 < lines.size()) {
            lines[i].end_ms = std::max(lines[i].start_ms, lines[i + 1].start_ms - 1);
        }
    }

    return lines;
}

} // namespace replayer
