#pragma once

#include <atomic>
#include <filesystem>
#include <thread>
#include <vector>

#include "recording/recorder.h"
#include "recording/wav_writer.h"

namespace replayer {

class WasapiRecorder final : public IRecorder {
public:
    WasapiRecorder();
    ~WasapiRecorder() override;

    Result<void> Start(int sentence_id, const std::filesystem::path& output_path) override;
    Result<void> Stop() override;
    bool IsRecording() const override;

private:
    void CaptureLoop();

    std::atomic<bool> recording_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread worker_;
    WavWriter writer_;
    std::filesystem::path output_path_;
    int sentence_id_{-1};
};

} // namespace replayer
