#pragma once

#include <utility>

#include "core/error.h"

namespace replayer {

template <typename T>
class Result {
public:
    Result(T value) : value_(std::move(value)) {}
    Result(AppError error) : error_(std::move(error)), ok_(false) {}

    [[nodiscard]] bool Ok() const noexcept {
        return ok_;
    }

    [[nodiscard]] const T& Value() const {
        return value_;
    }

    [[nodiscard]] T& Value() {
        return value_;
    }

    [[nodiscard]] const AppError& Error() const {
        return error_;
    }

private:
    T value_{};
    AppError error_{};
    bool ok_{true};
};

template <>
class Result<void> {
public:
    Result() = default;
    Result(AppError error) : error_(std::move(error)), ok_(false) {}

    [[nodiscard]] bool Ok() const noexcept {
        return ok_;
    }

    [[nodiscard]] const AppError& Error() const {
        return error_;
    }

private:
    AppError error_{};
    bool ok_{true};
};

inline Result<void> Ok() {
    return Result<void>();
}

inline AppError MakeError(ErrorCode code, std::wstring message) {
    return AppError{code, std::move(message)};
}

} // namespace replayer
