#pragma once

#include <cstdint>
#include <string>

namespace replayer {

struct SubtitleLine {
    int index{0};
    std::int64_t start_ms{0};
    std::int64_t end_ms{0};
    std::wstring text;
};

} // namespace replayer
