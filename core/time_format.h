#pragma once

#include <cstdint>
#include <string>

namespace replayer {

[[nodiscard]] std::wstring FormatTimestamp(std::int64_t milliseconds);

} // namespace replayer
