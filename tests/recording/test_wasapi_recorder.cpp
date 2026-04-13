#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include <gtest/gtest.h>

#include "recording/wasapi_recorder.h"

namespace replayer {
namespace {

TEST(WasapiRecorderTest, WritesSilentWavFile) {
    const auto path = std::filesystem::temp_directory_path() / L"replayer-recorder-test.wav";
    WasapiRecorder recorder;

    ASSERT_TRUE(recorder.Start(1, path).Ok());
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    ASSERT_TRUE(recorder.Stop().Ok());
    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_GT(std::filesystem::file_size(path), 44U);
}

} // namespace
} // namespace replayer
