#include "utils/logger.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace replayer {

namespace {

std::wstring TimestampNow() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &time);
#else
    local_tm = *std::localtime(&time);
#endif

    std::wstringstream stream;
    stream << std::put_time(&local_tm, L"%Y-%m-%d %H:%M:%S");
    return stream.str();
}

} // namespace

Logger::Logger(std::filesystem::path log_path) : log_path_(std::move(log_path)) {
    const auto parent = log_path_.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
    }
}

void Logger::Info(const std::wstring& message) {
    WriteLine(L"INFO", message);
}

void Logger::Error(const std::wstring& message) {
    WriteLine(L"ERROR", message);
}

const std::filesystem::path& Logger::LogPath() const noexcept {
    return log_path_;
}

void Logger::WriteLine(const std::wstring& level, const std::wstring& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto parent = log_path_.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);
    }

    std::wofstream stream(log_path_, std::ios::app);
    if (!stream.is_open()) {
        return;
    }

    stream << L"[" << TimestampNow() << L"] [" << level << L"] " << message << L"\n";
}

} // namespace replayer
