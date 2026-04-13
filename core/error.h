#pragma once

#include <string>

namespace replayer {

enum class ErrorCode {
    None = 0,
    InvalidArgument,
    InvalidState,
    FileNotFound,
    ParseError,
    IoError,
    NotSupported,
    DeviceUnavailable,
    SystemError
};

struct AppError {
    ErrorCode code{ErrorCode::None};
    std::wstring message;

    [[nodiscard]] bool HasError() const noexcept {
        return code != ErrorCode::None;
    }
};

} // namespace replayer
