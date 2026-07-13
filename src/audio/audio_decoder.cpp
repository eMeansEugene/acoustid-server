//
// AudioDecoder: MP3/WAV -> mono float samples.
//

#include "audio_decoder.h"

#include <cstring>
#include <fstream>
#include <stdexcept>

// dr_wav: single-header, implementation в этом TU.
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

// minimp3: single-header, implementation в этом TU.
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

namespace aid::audio {

// --- Формат по магическим байтам ------------------------------------------

AudioDecoder::Format AudioDecoder::DetectFormat(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 4) {
        return Format::kUnknown;
    }

    // WAV: начинается с "RIFF".
    if (std::memcmp(bytes.data(), "RIFF", 4) == 0) {
        return Format::kWav;
    }

    // MP3: ID3v2 тег ("ID3") или MPEG sync word (0xFF 0xE0 с маской).
    if (bytes.size() >= 3 && std::memcmp(bytes.data(), "ID3", 3) == 0) {
        return Format::kMp3;
    }
    if (bytes.size() >= 2 && bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0) {
        return Format::kMp3;
    }

    return Format::kUnknown;
}

// --- WAV ------------------------------------------------------------------

AudioData AudioDecoder::DecodeWav(const std::vector<uint8_t>& bytes) {
    drwav wav;
    if (!drwav_init_memory(&wav, bytes.data(), bytes.size(), nullptr)) {
        throw std::runtime_error("Failed to decode WAV: invalid or corrupted data");
    }

    const std::size_t num_channels = wav.channels;
    const std::size_t sample_rate = wav.sampleRate;
    const std::size_t total_frames = wav.totalPCMFrameCount;
    const std::size_t total_samples = total_frames * num_channels;

    // Декодируем все фреймы в float (интерливленные каналы).
    std::vector<float> interleaved(total_samples);
    const auto frames_read = drwav_read_pcm_frames_f32(&wav, total_frames, interleaved.data());
    drwav_uninit(&wav);

    if (frames_read == 0) {
        throw std::runtime_error("Failed to decode WAV: no frames read");
    }

    const std::size_t actual_samples = frames_read * num_channels;
    std::vector<float> mono = ExtractFirstChannel(interleaved.data(), actual_samples, num_channels);

    const float duration = static_cast<float>(mono.size()) / static_cast<float>(sample_rate);

    return AudioData{std::move(mono), sample_rate, duration};
}

// --- MP3 ------------------------------------------------------------------

AudioData AudioDecoder::DecodeMp3(const std::vector<uint8_t>& bytes) {
    mp3dec_ex_t dec{};
    if (mp3dec_ex_open_buf(&dec, bytes.data(), bytes.size(), MP3D_SEEK_TO_SAMPLE) != 0) {
        throw std::runtime_error("Failed to decode MP3: invalid or corrupted data");
    }

    const std::size_t num_channels = dec.info.channels;
    const std::size_t sample_rate = dec.info.hz;
    const std::size_t total_samples = dec.samples;  // total interleaved samples

    if (num_channels == 0 || sample_rate == 0 || total_samples == 0) {
        mp3dec_ex_close(&dec);
        throw std::runtime_error("Failed to decode MP3: empty or invalid stream");
    }

    std::vector<mp3d_sample_t> pcm(total_samples);
    const auto samples_read = mp3dec_ex_read(&dec, pcm.data(), total_samples);
    mp3dec_ex_close(&dec);

    if (samples_read == 0) {
        throw std::runtime_error("Failed to decode MP3: no samples read");
    }

    // minimp3 декодирует в int16_t, нормализуем в float [-1, 1].
    std::vector<float> interleaved(samples_read);
    for (std::size_t i = 0; i < samples_read; ++i) {
        interleaved[i] = static_cast<float>(pcm[i]) / 32768.0F;
    }

    std::vector<float> mono = ExtractFirstChannel(interleaved.data(), samples_read, num_channels);

    const float duration = static_cast<float>(mono.size()) / static_cast<float>(sample_rate);

    return AudioData{std::move(mono), sample_rate, duration};
}

// --- Первый канал ---------------------------------------------------------

std::vector<float> AudioDecoder::ExtractFirstChannel(const float* interleaved,
                                                      const std::size_t total_samples,
                                                      const std::size_t num_channels) {
    if (num_channels == 1) {
        return {interleaved, interleaved + total_samples};
    }

    const std::size_t num_frames = total_samples / num_channels;
    std::vector<float> mono(num_frames);
    for (std::size_t i = 0; i < num_frames; ++i) {
        mono[i] = interleaved[i * num_channels];
    }
    return mono;
}

// --- Публичные методы -----------------------------------------------------

AudioData AudioDecoder::DecodeFromBytes(const std::vector<uint8_t>& bytes) const {
    const Format format = DetectFormat(bytes);
    switch (format) {
        case Format::kWav:
            return DecodeWav(bytes);
        case Format::kMp3:
            return DecodeMp3(bytes);
        default:
            throw std::runtime_error("Unknown audio format: expected WAV or MP3");
    }
}

AudioData AudioDecoder::DecodeFromFile(const std::string& path) const {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    const auto size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error("File is empty or unreadable: " + path);
    }

    std::vector<uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(bytes.data()), size);

    return DecodeFromBytes(bytes);
}

}  // namespace aid::audio