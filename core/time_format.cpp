#include "core/time_format.h"

#include <iomanip>
#include <sstream>

namespace replayer {

std::wstring FormatTimestamp(std::int64_t milliseconds) {
    if (milliseconds < 0) {
        milliseconds = 0;
    }

    const auto total_seconds = milliseconds / 1000;
    const auto minutes = total_seconds / 60;
    const auto seconds = total_seconds % 60;

    std::wstringstream stream;
    stream << std::setfill(L'0') << std::setw(2) << minutes << L":" << std::setw(2) << seconds;
    return stream.str();
}

} // namespace replayer
