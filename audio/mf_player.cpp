#include "audio/mf_player.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <sstream>

namespace replayer {

namespace {

std::wstring EscapePath(const std::filesystem::path& path) {
    return path.wstring();
}

} // namespace

MfPlayer::MfPlayer() : alias_(L"replayer_mci") {}

MfPlayer::~MfPlayer() {
    CloseAlias();
}

Result<void> MfPlayer::Open(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return MakeError(ErrorCode::FileNotFound, L"Audio file not found.");
    }

    CloseAlias();
    path_ = path;

    std::wstringstream command;
    command << L"open \"" << EscapePath(path) << L"\" type mpegvideo alias " << alias_;
    if (const auto result = Execute(command.str()); !result.Ok()) {
        return result;
    }
    if (const auto result = Execute(L"set " + alias_ + L" time format milliseconds"); !result.Ok()) {
        return result;
    }

    std::wstring length_text;
    if (const auto result = Execute(L"status " + alias_ + L" length", true, &length_text); !result.Ok()) {
        return result;
    }

    duration_ms_ = _wtoi64(length_text.c_str());
    cached_position_ms_ = 0;
    state_ = PlaybackState::Stopped;
    opened_ = true;
    return Ok();
}

Result<void> MfPlayer::Play() {
    if (!opened_) {
        return MakeError(ErrorCode::InvalidState, L"No audio file is open.");
    }
    if (const auto result = Execute(L"play " + alias_); !result.Ok()) {
        return result;
    }
    state_ = PlaybackState::Playing;
    return Ok();
}

Result<void> MfPlayer::Pause() {
    if (!opened_) {
        return MakeError(ErrorCode::InvalidState, L"No audio file is open.");
    }
    if (const auto result = Execute(L"pause " + alias_); !result.Ok()) {
        return result;
    }
    state_ = PlaybackState::Paused;
    return Ok();
}

Result<void> MfPlayer::Stop() {
    if (!opened_) {
        return MakeError(ErrorCode::InvalidState, L"No audio file is open.");
    }
    if (const auto result = Execute(L"stop " + alias_); !result.Ok()) {
        return result;
    }
    cached_position_ms_ = 0;
    state_ = PlaybackState::Stopped;
    return Seek(0);
}

Result<void> MfPlayer::Seek(std::int64_t position_ms) {
    if (!opened_) {
        return MakeError(ErrorCode::InvalidState, L"No audio file is open.");
    }
    position_ms = std::clamp<std::int64_t>(position_ms, 0, duration_ms_);
    std::wstringstream command;
    command << L"seek " << alias_ << L" to " << position_ms;
    if (const auto result = Execute(command.str()); !result.Ok()) {
        return result;
    }
    cached_position_ms_ = position_ms;
    return Ok();
}

Result<void> MfPlayer::PlaySegment(std::int64_t start_ms, std::int64_t end_ms) {
    if (!opened_) {
        return MakeError(ErrorCode::InvalidState, L"No audio file is open.");
    }
    start_ms = std::clamp<std::int64_t>(start_ms, 0, duration_ms_);
    end_ms = std::clamp<std::int64_t>(end_ms, start_ms, duration_ms_);

    std::wstringstream command;
    command << L"play " << alias_ << L" from " << start_ms << L" to " << end_ms;
    if (const auto result = Execute(command.str()); !result.Ok()) {
        return result;
    }
    state_ = PlaybackState::Playing;
    cached_position_ms_ = start_ms;
    return Ok();
}

std::int64_t MfPlayer::GetPosition() const {
    RefreshPosition();
    return cached_position_ms_;
}

std::int64_t MfPlayer::GetDuration() const {
    return duration_ms_;
}

PlaybackState MfPlayer::GetState() const {
    return state_;
}

Result<void> MfPlayer::Execute(const std::wstring& command, bool expect_result, std::wstring* result) const {
    wchar_t buffer[256]{};
    const auto error = mciSendStringW(command.c_str(), expect_result ? buffer : nullptr, static_cast<UINT>(std::size(buffer)), nullptr);
    if (error != 0) {
        wchar_t message[256]{};
        mciGetErrorStringW(error, message, static_cast<UINT>(std::size(message)));
        return MakeError(ErrorCode::SystemError, message);
    }

    if (expect_result && result != nullptr) {
        *result = buffer;
    }

    return Ok();
}

Result<void> MfPlayer::CloseAlias() {
    if (!opened_) {
        return Ok();
    }
    Execute(L"close " + alias_);
    opened_ = false;
    return Ok();
}

Result<void> MfPlayer::RefreshPosition() const {
    if (!opened_) {
        cached_position_ms_ = 0;
        return Ok();
    }

    std::wstring position_text;
    if (const auto result = Execute(L"status " + alias_ + L" position", true, &position_text); !result.Ok()) {
        return result;
    }
    cached_position_ms_ = _wtoi64(position_text.c_str());
    return Ok();
}

} // namespace replayer
