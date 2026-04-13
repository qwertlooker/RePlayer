#include "recording/wav_writer.h"

#include <cstring>

namespace replayer {

namespace {

template <typename T>
void WritePrimitive(std::ofstream& stream, T value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

} // namespace

Result<void> WavWriter::Create(const std::filesystem::path& path,
                               std::uint16_t channels,
                               std::uint32_t sample_rate,
                               std::uint16_t bits_per_sample) {
    path_ = path;
    channels_ = channels;
    sample_rate_ = sample_rate;
    bits_per_sample_ = bits_per_sample;
    data_size_ = 0;
    finalized_ = false;

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);

    stream_.open(path, std::ios::binary | std::ios::trunc);
    if (!stream_.is_open()) {
        return MakeError(ErrorCode::IoError, L"Failed to create WAV file.");
    }

    return WriteHeader();
}

Result<void> WavWriter::AppendSamples(const std::vector<std::byte>& samples) {
    return AppendSamples(samples.data(), samples.size());
}

Result<void> WavWriter::AppendSamples(const void* data, std::size_t bytes) {
    if (!stream_.is_open() || finalized_) {
        return MakeError(ErrorCode::InvalidState, L"WAV writer is not active.");
    }

    stream_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(bytes));
    if (!stream_) {
        return MakeError(ErrorCode::IoError, L"Failed to append WAV samples.");
    }

    data_size_ += static_cast<std::uint32_t>(bytes);
    return Ok();
}

Result<void> WavWriter::Finalize() {
    if (!stream_.is_open()) {
        return Ok();
    }

    if (const auto header_result = WriteHeader(); !header_result.Ok()) {
        return header_result;
    }

    stream_.close();
    finalized_ = true;
    return Ok();
}

Result<void> WavWriter::WriteHeader() {
    if (!stream_.is_open()) {
        return MakeError(ErrorCode::InvalidState, L"WAV stream is not open.");
    }

    const std::uint32_t byte_rate = sample_rate_ * channels_ * bits_per_sample_ / 8;
    const std::uint16_t block_align = static_cast<std::uint16_t>(channels_ * bits_per_sample_ / 8);
    const std::uint32_t riff_chunk_size = 36 + data_size_;

    stream_.seekp(0, std::ios::beg);
    stream_.write("RIFF", 4);
    WritePrimitive(stream_, riff_chunk_size);
    stream_.write("WAVE", 4);
    stream_.write("fmt ", 4);
    WritePrimitive(stream_, static_cast<std::uint32_t>(16));
    WritePrimitive(stream_, static_cast<std::uint16_t>(1));
    WritePrimitive(stream_, channels_);
    WritePrimitive(stream_, sample_rate_);
    WritePrimitive(stream_, byte_rate);
    WritePrimitive(stream_, block_align);
    WritePrimitive(stream_, bits_per_sample_);
    stream_.write("data", 4);
    WritePrimitive(stream_, data_size_);
    stream_.seekp(0, std::ios::end);

    if (!stream_) {
        return MakeError(ErrorCode::IoError, L"Failed to write WAV header.");
    }

    return Ok();
}

} // namespace replayer
