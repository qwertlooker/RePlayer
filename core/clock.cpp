#include "core/clock.h"

#include <chrono>

namespace replayer {

std::int64_t SystemClock::NowMs() const {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

} // namespace replayer
