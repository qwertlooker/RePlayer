#pragma once

#include <cstdint>

namespace replayer {

class IClock {
public:
    virtual ~IClock() = default;
    virtual std::int64_t NowMs() const = 0;
};

class SystemClock final : public IClock {
public:
    std::int64_t NowMs() const override;
};

} // namespace replayer
