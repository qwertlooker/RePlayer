#include "subtitles/subtitle_text_utils.h"

namespace replayer {

std::wstring PrepareSubtitleTextForDisplay(const std::wstring& text) {
    std::wstring output;
    output.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        const wchar_t ch = text[i];
        if (ch == L'\r') {
            continue;
        }

        if (ch == L'<') {
            const auto close = text.find(L'>', i + 1);
            if (close != std::wstring::npos) {
                i = close;
                continue;
            }
        }

        output.push_back(ch);
    }

    return output;
}

} // namespace replayer
