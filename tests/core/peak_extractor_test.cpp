//
// Created by evgen on 13.07.2026.
//
//
// Юнит-тесты для aid::core::PeakExtractor.
//
// Покрывает пункты из README (раздел "Тестирование" -> "Юнит тесты" -> PeakExtractor):
//   - спектр с известными максимумами даёт правильные пики
//   - контроль плотности работает корректно
// плюс граничные случаи: конфиг, порог, плато, краевые точки.

#include "core/peak_extractor.h"

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

namespace aid::core {
namespace {

// Хелпер: создаёт спектрограмму заданного размера, заполненную floor_db,
// и позволяет точечно выставлять значения через At().
Spectrogram MakeFlat(const std::size_t num_frames, const std::size_t num_bins, const float floor_db = -100.0F) {
    Spectrogram s(num_frames, num_bins);
    for (std::size_t f = 0; f < num_frames; ++f) {
        for (std::size_t b = 0; b < num_bins; ++b) {
            s.At(f, b) = floor_db;
        }
    }
    return s;
}

// Конфиг с минимально разумными значениями для тестов.
PeakExtractorConfig TestConfig() {
    PeakExtractorConfig config;
    config.frame_radius_ = 2;
    config.bin_radius_ = 2;
    config.offset_db_ = 6.0F;
    config.zone_frames_ = 10;
    config.peak_limit_ = 50;
    return config;
}

// --- Валидация конфига ----------------------------------------------------

TEST(PeakExtractorConfigTest, ZeroZoneFramesThrows) {
    PeakExtractorConfig config = TestConfig();
    config.zone_frames_ = 0;
    EXPECT_THROW(PeakExtractor{config}, std::invalid_argument);
}

TEST(PeakExtractorConfigTest, ValidConfigDoesNotThrow) {
    EXPECT_NO_THROW(PeakExtractor{TestConfig()});
}

// --- Спектрограмма меньше окна -> пустой результат -------------------------

TEST(PeakExtractorTest, TooFewFramesReturnsEmpty) {
    // 4 фрейма, frame_radius=2 -> нужно минимум 5 фреймов для хотя бы
    // одного кандидата (фрейм 2 — единственный центр, от 0 до 4).
    auto config = TestConfig();
    const PeakExtractor extractor(config);

    Spectrogram s = MakeFlat(4, 20);
    // Поставим яркую точку — всё равно не должна пройти, нет места для окна.
    s.At(2, 10) = 50.0F;

    const auto peaks = extractor.ExtractPeaks(s);
    EXPECT_TRUE(peaks.empty());
}

TEST(PeakExtractorTest, TooFewBinsReturnsEmpty) {
    auto config = TestConfig();
    const PeakExtractor extractor(config);

    Spectrogram s = MakeFlat(20, 4);
    s.At(10, 2) = 50.0F;

    const auto peaks = extractor.ExtractPeaks(s);
    EXPECT_TRUE(peaks.empty());
}

// --- Одиночный пик на ровном фоне -----------------------------------------

TEST(PeakExtractorTest, SingleBrightPointFoundAsPeak) {
    auto config = TestConfig();
    const PeakExtractor extractor(config);

    // Фон -100 дБ, один яркий пик 0 дБ.
    // Медиана фрейма ≈ -100, порог = -100 + 6 = -94 -> 0 дБ пройдёт.
    Spectrogram s = MakeFlat(10, 20, -100.0F);
    s.At(5, 10) = 0.0F;

    const auto peaks = extractor.ExtractPeaks(s);
    ASSERT_EQ(peaks.size(), 1U);
    EXPECT_EQ(peaks[0].frame_index_, 5U);
    EXPECT_EQ(peaks[0].bin_index_, 10U);
    EXPECT_FLOAT_EQ(peaks[0].p_max_, 0.0F);
}

// --- Точка ниже порога не проходит ----------------------------------------

TEST(PeakExtractorTest, PeakBelowThresholdIsRejected) {
    auto config = TestConfig();
    config.offset_db_ = 6.0F;
    const PeakExtractor extractor(config);

    // Фон 0 дБ, «пик» +5 дБ — он локальный максимум, но выше медианы
    // всего на 5 дБ, а порог 6 дБ -> не проходит.
    Spectrogram s = MakeFlat(10, 20, 0.0F);
    s.At(5, 10) = 5.0F;

    const auto peaks = extractor.ExtractPeaks(s);
    EXPECT_TRUE(peaks.empty());
}

TEST(PeakExtractorTest, PeakExactlyAtThresholdIsRejected) {
    auto config = TestConfig();
    config.offset_db_ = 6.0F;
    const PeakExtractor extractor(config);

    // Фон 0 дБ, «пик» ровно +6 дБ. Условие в коде: value < threshold
    // (строгое), значит ровно на пороге — не отсекается.
    // На самом деле при value == threshold оно пройдёт (не < threshold),
    // а потом IsLocalMax отработает нормально, т.к. 6 > 0 у всех соседей.
    Spectrogram s = MakeFlat(10, 20, 0.0F);
    s.At(5, 10) = 6.0F;

    const auto peaks = extractor.ExtractPeaks(s);
    ASSERT_EQ(peaks.size(), 1U);
    EXPECT_EQ(peaks[0].frame_index_, 5U);
    EXPECT_EQ(peaks[0].bin_index_, 10U);
}

// --- Краевые точки не рассматриваются -------------------------------------

TEST(PeakExtractorTest, PeakAtBorderIsIgnored) {
    auto config = TestConfig();
    config.frame_radius_ = 2;
    config.bin_radius_ = 2;
    const PeakExtractor extractor(config);

    // Яркие точки на самых краях спектрограммы — не должны стать пиками,
    // потому что для них окно 5x5 не помещается целиком.
    Spectrogram s = MakeFlat(10, 20, -100.0F);
    s.At(0, 10) = 50.0F;   // первый фрейм
    s.At(9, 10) = 50.0F;   // последний фрейм
    s.At(5, 0) = 50.0F;    // первый бин
    s.At(5, 19) = 50.0F;   // последний бин
    s.At(1, 10) = 50.0F;   // frame_index < frame_radius

    const auto peaks = extractor.ExtractPeaks(s);
    EXPECT_TRUE(peaks.empty());
}

// --- Плато: два соседних бина с одинаковой мощностью — ни один не пик ------

TEST(PeakExtractorTest, PlateauGivesNoPeak) {
    auto config = TestConfig();
    const PeakExtractor extractor(config);

    Spectrogram s = MakeFlat(10, 20, -100.0F);
    // Два соседних бина с одинаковым значением — строгое сравнение
    // в IsLocalMax означает, что ни один не считается пиком.
    s.At(5, 10) = 0.0F;
    s.At(5, 11) = 0.0F;

    const auto peaks = extractor.ExtractPeaks(s);
    EXPECT_TRUE(peaks.empty());
}

// --- Несколько пиков далеко друг от друга ---------------------------------

TEST(PeakExtractorTest, MultipleSeparatedPeaksAllFound) {
    auto config = TestConfig();
    const PeakExtractor extractor(config);

    Spectrogram s = MakeFlat(20, 30, -100.0F);
    // Три пика на расстоянии больше радиуса окна друг от друга.
    s.At(5, 10) = 0.0F;
    s.At(5, 20) = -5.0F;
    s.At(15, 15) = -10.0F;

    const auto peaks = extractor.ExtractPeaks(s);
    ASSERT_EQ(peaks.size(), 3U);

    // Результат отсортирован по (frame_index, bin_index).
    EXPECT_EQ(peaks[0].frame_index_, 5U);
    EXPECT_EQ(peaks[0].bin_index_, 10U);

    EXPECT_EQ(peaks[1].frame_index_, 5U);
    EXPECT_EQ(peaks[1].bin_index_, 20U);

    EXPECT_EQ(peaks[2].frame_index_, 15U);
    EXPECT_EQ(peaks[2].bin_index_, 15U);
}

// --- Контроль плотности: top-N на зону ------------------------------------

TEST(PeakExtractorTest, DensityControlKeepsTopNPerZone) {
    auto config = TestConfig();
    config.zone_frames_ = 10;
    config.peak_limit_ = 2;
    const PeakExtractor extractor(config);

    // 10 фреймов, 30 бинов — всё одна зона (зона 0: фреймы 0..9).
    // Ставим 4 пика, достаточно далеко друг от друга.
    Spectrogram s = MakeFlat(10, 30, -100.0F);
    s.At(3, 5) = 10.0F;    // самый яркий
    s.At(3, 15) = 5.0F;    // второй
    s.At(5, 10) = -20.0F;  // третий
    s.At(5, 20) = -30.0F;  // четвёртый

    const auto peaks = extractor.ExtractPeaks(s);

    // Лимит 2 на зону -> должны остаться два самых ярких.
    ASSERT_EQ(peaks.size(), 2U);
    EXPECT_FLOAT_EQ(peaks[0].p_max_, 10.0F);
    EXPECT_FLOAT_EQ(peaks[1].p_max_, 5.0F);
}

TEST(PeakExtractorTest, DensityControlAppliesToEachZoneSeparately) {
    auto config = TestConfig();
    config.zone_frames_ = 10;
    config.peak_limit_ = 1;
    const PeakExtractor extractor(config);

    // Две зоны: 0..9 и 10..19. В каждой по 2 пика, лимит 1.
    Spectrogram s = MakeFlat(20, 30, -100.0F);
    s.At(3, 10) = 10.0F;   // зона 0, ярче
    s.At(5, 20) = 5.0F;    // зона 0, тусклее
    s.At(13, 10) = 8.0F;   // зона 1, ярче
    s.At(15, 20) = 3.0F;   // зона 1, тусклее

    const auto peaks = extractor.ExtractPeaks(s);

    // По 1 пику на зону -> всего 2.
    ASSERT_EQ(peaks.size(), 2U);
    EXPECT_FLOAT_EQ(peaks[0].p_max_, 10.0F);
    EXPECT_FLOAT_EQ(peaks[1].p_max_, 8.0F);
}

TEST(PeakExtractorTest, DensityControlBelowLimitKeepsAll) {
    auto config = TestConfig();
    config.zone_frames_ = 10;
    config.peak_limit_ = 50;
    const PeakExtractor extractor(config);

    Spectrogram s = MakeFlat(10, 30, -100.0F);
    s.At(5, 10) = 0.0F;
    s.At(5, 20) = -5.0F;

    const auto peaks = extractor.ExtractPeaks(s);
    // Пиков меньше лимита — все на месте.
    EXPECT_EQ(peaks.size(), 2U);
}

// --- Результат отсортирован детерминированно ------------------------------

TEST(PeakExtractorTest, OutputIsSortedByFrameThenBin) {
    auto config = TestConfig();
    const PeakExtractor extractor(config);

    Spectrogram s = MakeFlat(20, 30, -100.0F);
    // Вставляем пики «не по порядку» (хотя обход и так построчный,
    // проверяем пост-сортировку после density control).
    s.At(15, 5) = 0.0F;
    s.At(5, 25) = -5.0F;
    s.At(5, 10) = -10.0F;

    const auto peaks = extractor.ExtractPeaks(s);
    ASSERT_EQ(peaks.size(), 3U);

    for (std::size_t i = 1; i < peaks.size(); ++i) {
        const bool ordered = (peaks[i].frame_index_ > peaks[i - 1].frame_index_) ||
                              (peaks[i].frame_index_ == peaks[i - 1].frame_index_ &&
                               peaks[i].bin_index_ > peaks[i - 1].bin_index_);
        EXPECT_TRUE(ordered) << "peaks[" << i - 1 << "] and peaks[" << i << "] out of order";
    }
}

}  // namespace
}  // namespace aid::core