//
// Created by evgen on 13.07.2026.
//
//
// Юнит-тесты для aid::audio::AudioDecoder.
//
// Тесты строят синтетические WAV-файлы в памяти (формат достаточно прост).
// MP3 синтетически не конструируется — тестируется только через
// определение формата и отклонение невалидных данных.

#include "audio/audio_decoder.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

namespace aid::audio {
namespace {

// ---- Хелпер: построение минимального PCM WAV в памяти --------------------

// Little-endian write helpers.
void WriteU16(std::vector<uint8_t>& buf, const uint16_t value) {
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void WriteU32(std::vector<uint8_t>& buf, const uint32_t value) {
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void WriteTag(std::vector<uint8_t>& buf, const char* tag) {
    for (int i = 0; i < 4; ++i) {
        buf.push_back(static_cast<uint8_t>(tag[i]));
    }
}

void WriteI16Sample(std::vector<uint8_t>& buf, const int16_t sample) {
    buf.push_back(static_cast<uint8_t>(sample & 0xFF));
    buf.push_back(static_cast<uint8_t>((sample >> 8) & 0xFF));
}

/// Строит валидный PCM WAV (16-bit) в памяти.
/// @param samples    Моно-сэмплы в диапазоне [-1, 1].
/// @param sample_rate Частота дискретизации.
/// @param num_channels 1 = моно, 2 = стерео (дублирует сэмплы в оба канала).
std::vector<uint8_t> BuildWavBytes(const std::vector<float>& samples,
                                    const uint32_t sample_rate,
                                    const uint16_t num_channels) {
    const uint16_t bits_per_sample = 16;
    const uint16_t block_align = num_channels * bits_per_sample / 8;
    const uint32_t byte_rate = sample_rate * block_align;
    const uint32_t data_size = static_cast<uint32_t>(samples.size()) * num_channels * (bits_per_sample / 8);
    const uint32_t file_size = 36 + data_size;

    std::vector<uint8_t> buf;
    buf.reserve(44 + data_size);

    // RIFF header
    WriteTag(buf, "RIFF");
    WriteU32(buf, file_size);
    WriteTag(buf, "WAVE");

    // fmt chunk
    WriteTag(buf, "fmt ");
    WriteU32(buf, 16);              // chunk size (PCM)
    WriteU16(buf, 1);               // audio format: PCM
    WriteU16(buf, num_channels);
    WriteU32(buf, sample_rate);
    WriteU32(buf, byte_rate);
    WriteU16(buf, block_align);
    WriteU16(buf, bits_per_sample);

    // data chunk
    WriteTag(buf, "data");
    WriteU32(buf, data_size);

    for (const float s : samples) {
        const auto clamped = std::max(-1.0F, std::min(1.0F, s));
        const auto value = static_cast<int16_t>(clamped * 32767.0F);
        WriteI16Sample(buf, value);
        if (num_channels == 2) {
            // Стерео: дублируем в правый канал (оба канала одинаковы).
            WriteI16Sample(buf, value);
        }
    }

    return buf;
}

/// Генерирует синусоиду заданной частоты.
std::vector<float> MakeSine(const float freq_hz, const float duration_sec, const uint32_t sample_rate) {
    const auto num_samples = static_cast<std::size_t>(duration_sec * static_cast<float>(sample_rate));
    std::vector<float> samples(num_samples);
    for (std::size_t i = 0; i < num_samples; ++i) {
        samples[i] = std::sin(2.0F * static_cast<float>(M_PI) * freq_hz *
                               static_cast<float>(i) / static_cast<float>(sample_rate));
    }
    return samples;
}

// --- Определение формата --------------------------------------------------

TEST(AudioDecoderTest, EmptyBytesThrows) {
    const AudioDecoder decoder;
    EXPECT_THROW(decoder.DecodeFromBytes({}), std::runtime_error);
}

TEST(AudioDecoderTest, GarbageBytesThrows) {
    const AudioDecoder decoder;
    const std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_THROW(decoder.DecodeFromBytes(garbage), std::runtime_error);
}

// --- WAV моно -------------------------------------------------------------

TEST(AudioDecoderTest, MonoWavDecodesCorrectly) {
    const AudioDecoder decoder;

    const uint32_t sample_rate = 44100;
    const auto sine = MakeSine(440.0F, 0.1F, sample_rate);
    const auto wav_bytes = BuildWavBytes(sine, sample_rate, 1);

    const AudioData result = decoder.DecodeFromBytes(wav_bytes);

    EXPECT_EQ(result.sample_rate_, sample_rate);
    EXPECT_EQ(result.samples_.size(), sine.size());
    EXPECT_NEAR(result.duration_sec_, 0.1F, 0.01F);
}

TEST(AudioDecoderTest, MonoWavSamplesMatchInput) {
    const AudioDecoder decoder;

    const uint32_t sample_rate = 44100;
    const auto sine = MakeSine(440.0F, 0.05F, sample_rate);
    const auto wav_bytes = BuildWavBytes(sine, sample_rate, 1);

    const AudioData result = decoder.DecodeFromBytes(wav_bytes);

    ASSERT_EQ(result.samples_.size(), sine.size());
    // 16-битная квантизация вносит ошибку до 1/32768 ≈ 3e-5.
    for (std::size_t i = 0; i < sine.size(); ++i) {
        EXPECT_NEAR(result.samples_[i], sine[i], 1e-4F) << "sample " << i;
    }
}

// --- WAV стерео: берётся первый канал -------------------------------------

TEST(AudioDecoderTest, StereoWavExtractsFirstChannel) {
    const AudioDecoder decoder;

    const uint32_t sample_rate = 22050;
    const auto sine = MakeSine(1000.0F, 0.05F, sample_rate);
    // BuildWavBytes дублирует моно в оба канала при num_channels=2.
    const auto wav_bytes = BuildWavBytes(sine, sample_rate, 2);

    const AudioData result = decoder.DecodeFromBytes(wav_bytes);

    // Выход — моно, количество сэмплов совпадает с исходным (не удвоенное).
    EXPECT_EQ(result.samples_.size(), sine.size());
    EXPECT_EQ(result.sample_rate_, sample_rate);

    for (std::size_t i = 0; i < sine.size(); ++i) {
        EXPECT_NEAR(result.samples_[i], sine[i], 1e-4F) << "sample " << i;
    }
}

// --- Разные sample rates --------------------------------------------------

TEST(AudioDecoderTest, DifferentSampleRatesPreserved) {
    const AudioDecoder decoder;

    for (const uint32_t sr : {8000U, 16000U, 22050U, 44100U, 48000U}) {
        const auto sine = MakeSine(200.0F, 0.05F, sr);
        const auto wav_bytes = BuildWavBytes(sine, sr, 1);

        const AudioData result = decoder.DecodeFromBytes(wav_bytes);
        EXPECT_EQ(result.sample_rate_, sr) << "sample_rate=" << sr;
    }
}

// --- Duration вычисляется корректно ---------------------------------------

TEST(AudioDecoderTest, DurationComputedCorrectly) {
    const AudioDecoder decoder;

    const uint32_t sample_rate = 44100;
    const float target_duration = 0.5F;
    const auto sine = MakeSine(440.0F, target_duration, sample_rate);
    const auto wav_bytes = BuildWavBytes(sine, sample_rate, 1);

    const AudioData result = decoder.DecodeFromBytes(wav_bytes);
    EXPECT_NEAR(result.duration_sec_, target_duration, 0.01F);
}

// --- Повреждённый WAV -----------------------------------------------------

TEST(AudioDecoderTest, TruncatedWavThrows) {
    const AudioDecoder decoder;

    const auto sine = MakeSine(440.0F, 0.1F, 44100);
    auto wav_bytes = BuildWavBytes(sine, 44100, 1);
    // Обрезаем до половины заголовка.
    wav_bytes.resize(20);

    EXPECT_THROW(decoder.DecodeFromBytes(wav_bytes), std::runtime_error);
}

// --- MP3: невалидные данные -----------------------------------------------

TEST(AudioDecoderTest, InvalidMp3Throws) {
    const AudioDecoder decoder;

    // Байты, похожие на начало MP3 (sync word), но дальше мусор.
    std::vector<uint8_t> fake_mp3 = {0xFF, 0xFB, 0x00, 0x00, 0x00, 0x00};
    // Это может либо бросить исключение, либо декодировать 0 сэмплов.
    // В обоих случаях — ожидаем runtime_error.
    EXPECT_THROW(decoder.DecodeFromBytes(fake_mp3), std::runtime_error);
}

// --- Файл: несуществующий путь --------------------------------------------

TEST(AudioDecoderTest, NonexistentFileThrows) {
    const AudioDecoder decoder;
    EXPECT_THROW(decoder.DecodeFromFile("/tmp/nonexistent_audio_file_12345.wav"), std::runtime_error);
}

}  // namespace
}  // namespace aid::audio