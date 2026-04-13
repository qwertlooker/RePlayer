#include "subtitles/srt_parser.h"

#include <algorithm>
#include <fstream>
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

std::int64_t ParseSrtTime(const std::wstring& time_text) {
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int millis = 0;
    if (swscanf_s(time_text.c_str(), L"%d:%d:%d,%d", &hours, &minutes, &seconds, &millis) != 4) {
        return -1;
    }
    return (((hours * 60LL) + minutes) * 60LL + seconds) * 1000LL + millis;
}

} // namespace

Result<std::vector<SubtitleLine>> SrtParser::ParseFile(const std::filesystem::path& path) const {
    if (!std::filesystem::exists(path)) {
        return MakeError(ErrorCode::FileNotFound, L"SRT file not found.");
    }
    return ParseText(ReadAllText(path));
}

Result<std::vector<SubtitleLine>> SrtParser::ParseText(const std::wstring& text) const {
    std::vector<SubtitleLine> lines;
    std::wistringstream stream(text);
    std::wstring line;

    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty()) {
            continue;
        }

        int index = 0;
        try {
            index = std::stoi(line);
        } catch (...) {
            return MakeError(ErrorCode::ParseError, L"Invalid SRT index.");
        }

        std::wstring time_range;
        if (!std::getline(stream, time_range)) {
            return MakeError(ErrorCode::ParseError, L"Missing SRT time range.");
        }

        const auto arrow = time_range.find(L" --> ");
        if (arrow == std::wstring::npos) {
            return MakeError(ErrorCode::ParseError, L"Invalid SRT time range.");
        }

        const auto start_ms = ParseSrtTime(Trim(time_range.substr(0, arrow)));
        const auto end_ms = ParseSrtTime(Trim(time_range.substr(arrow + 5)));
        if (start_ms < 0 || end_ms < start_ms) {
            return MakeError(ErrorCode::ParseError, L"Invalid SRT timestamp.");
        }

        std::wstring text_block;
        while (std::getline(stream, line)) {
            line = Trim(line);
            if (line.empty()) {
                break;
            }
            if (!text_block.empty()) {
                text_block += L"\n";
            }
            text_block += line;
        }

        lines.push_back(SubtitleLine{index, start_ms, end_ms, text_block});
    }

    return lines;
}

} // namespace replayer
