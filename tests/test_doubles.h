#pragma once

#include <filesystem>
#include <vector>

#include "audio/audio_player.h"
#include "core/clock.h"
#include "recording/recorder.h"

namespace replayer::tests {

class FakeClock final : public IClock {
public:
    std::int64_t NowMs() const override {
        return now_ms_;
    }

    void SetNowMs(std::int64_t value) {
        now_ms_ = value;
    }

private:
    std::int64_t now_ms_{0};
};

class FakeAudioPlayer final : public IAudioPlayer {
public:
    Result<void> Open(const std::filesystem::path& path) override {
        opened_path = path;
        position_ms = 0;
        state = PlaybackState::Stopped;
        return Ok();
    }

    Result<void> Play() override {
        state = PlaybackState::Playing;
        ++play_calls;
        return Ok();
    }

    Result<void> Pause() override {
        state = PlaybackState::Paused;
        ++pause_calls;
        return Ok();
    }

    Result<void> Stop() override {
        state = PlaybackState::Stopped;
        position_ms = 0;
        ++stop_calls;
        return Ok();
    }

    Result<void> Seek(std::int64_t value) override {
        position_ms = value;
        seek_history.push_back(value);
        return Ok();
    }

    Result<void> PlaySegment(std::int64_t start_ms, std::int64_t end_ms) override {
        segment_start = start_ms;
        segment_end = end_ms;
        state = PlaybackState::Playing;
        return Ok();
    }

    std::int64_t GetPosition() const override {
        return position_ms;
    }

    std::int64_t GetDuration() const override {
        return duration_ms;
    }

    PlaybackState GetState() const override {
        return state;
    }

    std::filesystem::path opened_path;
    std::int64_t position_ms{0};
    std::int64_t duration_ms{10000};
    PlaybackState state{PlaybackState::Stopped};
    std::vector<std::int64_t> seek_history;
    std::int64_t segment_start{0};
    std::int64_t segment_end{0};
    int play_calls{0};
    int pause_calls{0};
    int stop_calls{0};
};

class FakeRecorder final : public IRecorder {
public:
    Result<void> Start(int sentence_id, const std::filesystem::path& output_path) override {
        is_recording = true;
        started_sentence = sentence_id;
        started_path = output_path;
        return Ok();
    }

    Result<void> Stop() override {
        is_recording = false;
        ++stop_calls;
        return Ok();
    }

    bool IsRecording() const override {
        return is_recording;
    }

    bool is_recording{false};
    int started_sentence{-1};
    std::filesystem::path started_path;
    int stop_calls{0};
};

} // namespace replayer::tests
