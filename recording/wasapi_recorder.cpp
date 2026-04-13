#include "recording/wasapi_recorder.h"

#include <chrono>

namespace replayer {

WasapiRecorder::WasapiRecorder() = default;

WasapiRecorder::~WasapiRecorder() {
    Stop();
}

Result<void> WasapiRecorder::Start(int sentence_id, const std::filesystem::path& output_path) {
    if (recording_) {
        return MakeError(ErrorCode::InvalidState, L"Recorder is already active.");
    }

    sentence_id_ = sentence_id;
    output_path_ = output_path;
    stop_requested_ = false;

    if (const auto result = writer_.Create(output_path_, 1, 16000, 16); !result.Ok()) {
        return result;
    }

    recording_ = true;
    worker_ = std::thread(&WasapiRecorder::CaptureLoop, this);
    return Ok();
}

Result<void> WasapiRecorder::Stop() {
    if (!recording_) {
        return Ok();
    }

    stop_requested_ = true;
    if (worker_.joinable()) {
        worker_.join();
    }

    recording_ = false;
    return writer_.Finalize();
}

bool WasapiRecorder::IsRecording() const {
    return recording_;
}

void WasapiRecorder::CaptureLoop() {
    // Keep the first implementation predictable for local testing by writing silence.
    // The recorder interface stays compatible with a future WASAPI-backed capture path.
    const std::vector<std::byte> silence_chunk(1600, std::byte{0});
    while (!stop_requested_) {
        writer_.AppendSamples(silence_chunk);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

} // namespace replayer
