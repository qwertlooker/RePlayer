#pragma once

#include <filesystem>
#include <mutex>
#include <string>

namespace replayer {

class Logger {
public:
    explicit Logger(std::filesystem::path log_path = std::filesystem::path(L"logs") / L"app.log");

    void Info(const std::wstring& message);
    void Error(const std::wstring& message);
    [[nodiscard]] const std::filesystem::path& LogPath() const noexcept;

private:
    void WriteLine(const std::wstring& level, const std::wstring& message);

    std::filesystem::path log_path_;
    std::mutex mutex_;
};

} // namespace replayer
