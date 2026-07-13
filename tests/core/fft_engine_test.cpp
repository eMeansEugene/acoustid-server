//
#include "core/fft_engine.h"

#include <cmath>
#include <vector>

#include <gtest/gtest.h>

namespace aid::core {
namespace {

// Значение out_row для нулевого сигнала: 10*log10(power + eps), power=0.
// LOG_POWER_EPSILON = 1e-10 задан в fft_engine.cpp (внутренний, не виден
// из заголовка), поэтому продублируем его здесь как ожидаемую константу.
constexpr float EXPECTED_ZERO_POWER_DB = -100.0F;  // 10 * log10(1e-10)
constexpr float EPSILON = 1e-3F;

// Генерирует синусоиду, содержащую ровно `cycles` полных периодов на
// `length` сэмплов: x[n] = sin(2*pi*cycles*n / length).
// Это гарантирует, что частота сигнала совпадает точно с одним из бинов
// r2c-FFT размера `length`, без спектральной утечки на бин.
std::vector<float> MakeExactBinSine(std::size_t length, std::size_t cycles) {
    std::vector<float> samples(length);
    const auto n_total = static_cast<float>(length);
    for (std::size_t n = 0; n < length; ++n) {
        samples[n] =
            std::sin(2.0F * static_cast<float>(M_PI) * static_cast<float>(cycles) * static_cast<float>(n) / n_total);
    }
    return samples;
}

std::size_t ArgMaxBin(const Spectrogram& spectrogram, std::size_t frame) {
    std::size_t best_bin = 0;
    float best_value = spectrogram.At(frame, 0);
    for (std::size_t bin = 1; bin < spectrogram.NumBins(); ++bin) {
        const float value = spectrogram.At(frame, bin);
        if (value > best_value) {
            best_value = value;
            best_bin = bin;
        }
    }
    return best_bin;
}

// --- Конфигурация ---------------------------------------------------------

TEST(FftEngineConfigTest, ZeroFrameSizeThrows) {
    FftEngineConfig config;
    config.frame_size_ = 0;
    EXPECT_THROW(FftEngine{config}, std::invalid_argument);
}

TEST(FftEngineConfigTest, OddFrameSizeThrows) {
    FftEngineConfig config;
    config.frame_size_ = 2047;
    EXPECT_THROW(FftEngine{config}, std::invalid_argument);
}

TEST(FftEngineConfigTest, ZeroHopSizeThrows) {
    FftEngineConfig config;
    config.hop_size_ = 0;
    EXPECT_THROW(FftEngine{config}, std::invalid_argument);
}

TEST(FftEngineConfigTest, ValidConfigDoesNotThrow) {
    FftEngineConfig config;
    config.frame_size_ = 1024;
    config.hop_size_ = 512;
    EXPECT_NO_THROW(FftEngine{config});
}

TEST(FftEngineConfigTest, NumBinsIsHalfFrameSize) {
    FftEngineConfig config;
    config.frame_size_ = 2048;
    const FftEngine engine(config);
    EXPECT_EQ(engine.NumBins(), 1024U);
}

// --- Размер входа / число фреймов -----------------------------------------

TEST(FftEngineTest, ShorterThanFrameGivesZeroFrames) {
    FftEngineConfig config;
    config.frame_size_ = 2048;
    config.hop_size_ = 1024;
    const FftEngine engine(config);

    const std::vector<float> samples(2047, 0.0F);
    const Spectrogram spectrogram = engine.ComputeSpectrogram(samples);

    EXPECT_EQ(spectrogram.NumFrames(), 0U);
    EXPECT_EQ(spectrogram.NumBins(), engine.NumBins());
}

TEST(FftEngineTest, ExactlyOneFrameGivesOneFrame) {
    FftEngineConfig config;
    config.frame_size_ = 2048;
    config.hop_size_ = 1024;
    const FftEngine engine(config);

    const std::vector<float> samples(2048, 0.0F);
    const Spectrogram spectrogram = engine.ComputeSpectrogram(samples);

    EXPECT_EQ(spectrogram.NumFrames(), 1U);
}

TEST(FftEngineTest, NumFramesMatchesOverlapFormula) {
    FftEngineConfig config;
    config.frame_size_ = 2048;
    config.hop_size_ = 1024;
    const FftEngine engine(config);

    // 2048 + 3*1024 сэмплов -> (N - frame)/hop + 1 = (5120-2048)/1024+1 = 4
    const std::vector<float> samples(2048 + 3 * 1024, 0.0F);
    const Spectrogram spectrogram = engine.ComputeSpectrogram(samples);

    EXPECT_EQ(spectrogram.NumFrames(), 4U);
}

// --- Нулевой сигнал ---------------------------------------------------------

TEST(FftEngineTest, ZeroSignalGivesMinimalPowerEverywhere) {
    FftEngineConfig config;
    config.frame_size_ = 2048;
    config.hop_size_ = 1024;
    const FftEngine engine(config);

    const std::vector<float> samples(2048 * 3, 0.0F);
    const Spectrogram spectrogram = engine.ComputeSpectrogram(samples);

    ASSERT_GT(spectrogram.NumFrames(), 0U);
    for (std::size_t frame = 0; frame < spectrogram.NumFrames(); ++frame) {
        for (std::size_t bin = 0; bin < spectrogram.NumBins(); ++bin) {
            EXPECT_NEAR(spectrogram.At(frame, bin), EXPECTED_ZERO_POWER_DB, EPSILON)
                << "frame=" << frame << " bin=" << bin;
        }
    }
}

// --- Синус даёт пик на своей частоте ---------------------------------------

TEST(FftEngineTest, SineGivesPeakAtExpectedBin) {
    FftEngineConfig config;
    config.frame_size_ = 2048;
    config.hop_size_ = 1024;
    const FftEngine engine(config);

    // j циклов на frame_size сэмплов -> пик r2c-бина j, после отбрасывания
    // DC (j=0) в ProcessFrame это соответствует out_row индексу j-1.
    constexpr std::size_t kCycles = 100;
    const std::vector<float> samples = MakeExactBinSine(config.frame_size_, kCycles);

    const Spectrogram spectrogram = engine.ComputeSpectrogram(samples);
    ASSERT_EQ(spectrogram.NumFrames(), 1U);

    const std::size_t expected_bin = kCycles - 1;
    EXPECT_EQ(ArgMaxBin(spectrogram, 0), expected_bin);

    // Пик должен заметно превышать фон (тишину/шум окна на других бинах).
    const float peak_value = spectrogram.At(0, expected_bin);
    const float neighbor_value = spectrogram.At(0, expected_bin + 5);
    EXPECT_GT(peak_value, neighbor_value + 10.0F);
}

TEST(FftEngineTest, SinePeakIsConsistentAcrossFrames) {
    FftEngineConfig config;
    config.frame_size_ = 2048;
    config.hop_size_ = 1024;
    const FftEngine engine(config);

    // Периодический (по построению) синус на несколько фреймов вперёд:
    // при hop = frame_size/2 сигнал с целым числом циклов на frame_size
    // остаётся периодичным с тем же шагом, так что пик должен быть
    // на одном и том же бине в каждом фрейме.
    constexpr std::size_t kCycles = 50;
    const std::size_t total_length = config.frame_size_ + 4 * config.hop_size_;
    std::vector<float> samples(total_length);
    for (std::size_t n = 0; n < total_length; ++n) {
        samples[n] = std::sin(2.0F * static_cast<float>(M_PI) * static_cast<float>(kCycles) * static_cast<float>(n) /
                               static_cast<float>(config.frame_size_));
    }

    const Spectrogram spectrogram = engine.ComputeSpectrogram(samples);
    ASSERT_GT(spectrogram.NumFrames(), 1U);

    const std::size_t expected_bin = kCycles - 1;
    for (std::size_t frame = 0; frame < spectrogram.NumFrames(); ++frame) {
        EXPECT_EQ(ArgMaxBin(spectrogram, frame), expected_bin) << "frame=" << frame;
    }
}

TEST(FftEngineTest, HigherFrequencyGivesHigherBin) {
    FftEngineConfig config;
    config.frame_size_ = 2048;
    config.hop_size_ = 1024;
    const FftEngine engine(config);

    const std::vector<float> low = MakeExactBinSine(config.frame_size_, 20);
    const std::vector<float> high = MakeExactBinSine(config.frame_size_, 200);

    const Spectrogram low_spectrogram = engine.ComputeSpectrogram(low);
    const Spectrogram high_spectrogram = engine.ComputeSpectrogram(high);

    EXPECT_LT(ArgMaxBin(low_spectrogram, 0), ArgMaxBin(high_spectrogram, 0));
}

}  // namespace
}  // namespace aid::core