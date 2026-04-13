#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

#include "recording/wav_writer.h"

namespace replayer {
namespace {

TEST(WavWriterTest, WritesStandardHeader) {
    const auto path = std::filesystem::temp_directory_path() / L"replayer-test.wav";
    WavWriter writer;
    ASSERT_TRUE(writer.Create(path, 1, 16000, 16).Ok());

    const std::array<std::byte, 8> samples{
        std::byte{0x01}, std::byte{0x00}, std::byte{0x02}, std::byte{0x00},
        std::byte{0x03}, std::byte{0x00}, std::byte{0x04}, std::byte{0x00}
    };
    ASSERT_TRUE(writer.AppendSamples(samples.data(), samples.size()).Ok());
    ASSERT_TRUE(writer.Finalize().Ok());

    std::ifstream stream(path, std::ios::binary);
    ASSERT_TRUE(stream.is_open());
    std::string header(12, '\0');
    stream.read(header.data(), static_cast<std::streamsize>(header.size()));

    EXPECT_EQ(header.substr(0, 4), "RIFF");
    EXPECT_EQ(header.substr(8, 4), "WAVE");
}

} // namespace
} // namespace replayer
