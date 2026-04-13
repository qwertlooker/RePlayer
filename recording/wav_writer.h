#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "core/result.h"

namespace replayer {

class WavWriter {
public:
    Result<void> Create(const std::filesystem::path& path,
                        std::uint16_t channels,
                        std::uint32_t sample_rate,
                        std::uint16_t bits_per_sample);
    Result<void> AppendSamples(const std::vector<std::byte>& samples);
    Result<void> AppendSamples(const void* data, std::size_t bytes);
    Result<void> Finalize();

private:
    Result<void> WriteHeader();

    std::filesystem::path path_;
    std::ofstream stream_;
    std::uint16_t channels_{1};
    std::uint32_t sample_rate_{16000};
    std::uint16_t bits_per_sample_{16};
    std::uint32_t data_size_{0};
    bool finalized_{true};
};

} // namespace replayer
