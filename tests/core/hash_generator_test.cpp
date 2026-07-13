//
// Created by evgen on 13.07.2026.
//
//
// Юнит-тесты для aid::core::HashGenerator.
//
// Покрывает пункты из README:
//   - Корректность упаковки трёх чисел в uint32_t
//   - Обратимость кодирования
// плюс: фильтрация верхних частот, лимит целей на якорь, граничные случаи.

#include "core/hash_generator.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace aid::core {
namespace {

// --- Валидация конфига ----------------------------------------------------

TEST(HashGeneratorConfigTest, ZeroTargetsPerAnchorThrows) {
    HashGeneratorConfig config;
    config.max_targets_per_anchor_ = 0;
    EXPECT_THROW(HashGenerator{config}, std::invalid_argument);
}

TEST(HashGeneratorConfigTest, FreqBinLimitZeroThrows) {
    HashGeneratorConfig config;
    config.freq_bin_limit_ = 0;
    EXPECT_THROW(HashGenerator{config}, std::invalid_argument);
}

TEST(HashGeneratorConfigTest, FreqBinLimitOver512Throws) {
    HashGeneratorConfig config;
    config.freq_bin_limit_ = 513;
    EXPECT_THROW(HashGenerator{config}, std::invalid_argument);
}

TEST(HashGeneratorConfigTest, ValidConfigDoesNotThrow) {
    EXPECT_NO_THROW(HashGenerator{});
}

// --- PackHash / UnpackHash ------------------------------------------------

TEST(HashGeneratorTest, PackUnpackRoundtrip) {
    // Проверяем обратимость для нескольких характерных значений.
    struct Case {
        std::size_t freq_anchor;
        std::size_t freq_target;
        std::size_t time_delta;
    };
    const Case cases[] = {
        {0, 0, 0},
        {511, 511, 16383},    // максимумы 9+9+14 бит
        {100, 200, 50},
        {1, 1, 1},
        {256, 128, 8000},
        {0, 511, 0},
        {511, 0, 16383},
    };

    for (const auto& [fa, ft, dt] : cases) {
        const uint32_t hash = HashGenerator::PackHash(fa, ft, dt);
        const auto unpacked = HashGenerator::UnpackHash(hash);
        EXPECT_EQ(unpacked.freq_anchor, fa) << "fa=" << fa << " ft=" << ft << " dt=" << dt;
        EXPECT_EQ(unpacked.freq_target, ft) << "fa=" << fa << " ft=" << ft << " dt=" << dt;
        EXPECT_EQ(unpacked.time_delta, dt) << "fa=" << fa << " ft=" << ft << " dt=" << dt;
    }
}

TEST(HashGeneratorTest, PackUsesAll32Bits) {
    // Все биты установлены в 1: 511 | 511 | 16383 -> 0xFFFFFFFF.
    const uint32_t hash = HashGenerator::PackHash(511, 511, 16383);
    EXPECT_EQ(hash, 0xFFFFFFFFU);
}

TEST(HashGeneratorTest, PackZerosGiveZero) {
    const uint32_t hash = HashGenerator::PackHash(0, 0, 0);
    EXPECT_EQ(hash, 0U);
}

TEST(HashGeneratorTest, DifferentInputsGiveDifferentHashes) {
    const uint32_t h1 = HashGenerator::PackHash(100, 200, 50);
    const uint32_t h2 = HashGenerator::PackHash(100, 201, 50);
    const uint32_t h3 = HashGenerator::PackHash(101, 200, 50);
    const uint32_t h4 = HashGenerator::PackHash(100, 200, 51);
    EXPECT_NE(h1, h2);
    EXPECT_NE(h1, h3);
    EXPECT_NE(h1, h4);
}

// --- Generate: базовый случай ---------------------------------------------

TEST(HashGeneratorTest, TwoPeaksProduceOneFingerprint) {
    HashGeneratorConfig config;
    config.max_target_offset_ = 10;
    config.max_targets_per_anchor_ = 5;
    const HashGenerator gen(config);

    std::vector<Peak> peaks = {
        {0.0F, 0, 100},   // якорь
        {0.0F, 3, 200},   // цель, delta=3
    };

    const auto fps = gen.Generate(peaks);
    ASSERT_EQ(fps.size(), 1U);
    EXPECT_EQ(fps[0].anchor_frame_, 0U);

    const auto unpacked = HashGenerator::UnpackHash(fps[0].hash_);
    EXPECT_EQ(unpacked.freq_anchor, 100U);
    EXPECT_EQ(unpacked.freq_target, 200U);
    EXPECT_EQ(unpacked.time_delta, 3U);
}

TEST(HashGeneratorTest, SameFramePeaksAreValidTargets) {
    HashGeneratorConfig config;
    config.max_target_offset_ = 10;
    config.max_targets_per_anchor_ = 5;
    const HashGenerator gen(config);

    // Два пика в одном фрейме — delta=0, должны спариться.
    std::vector<Peak> peaks = {
        {0.0F, 5, 100},
        {0.0F, 5, 200},
    };

    const auto fps = gen.Generate(peaks);
    ASSERT_EQ(fps.size(), 1U);

    const auto unpacked = HashGenerator::UnpackHash(fps[0].hash_);
    EXPECT_EQ(unpacked.time_delta, 0U);
}

// --- Фильтрация верхних частот -------------------------------------------

TEST(HashGeneratorTest, PeaksAboveFreqLimitAreIgnored) {
    HashGeneratorConfig config;
    config.max_target_offset_ = 10;
    config.max_targets_per_anchor_ = 5;
    config.freq_bin_limit_ = 512;
    const HashGenerator gen(config);

    std::vector<Peak> peaks = {
        {0.0F, 0, 100},   // валидный
        {0.0F, 3, 512},   // bin_index == 512 -> отброшен
        {0.0F, 5, 600},   // bin_index > 512 -> отброшен
    };

    const auto fps = gen.Generate(peaks);
    // Единственный валидный пик — якорь без целей.
    EXPECT_TRUE(fps.empty());
}

TEST(HashGeneratorTest, MixedValidAndInvalidBins) {
    HashGeneratorConfig config;
    config.max_target_offset_ = 10;
    config.max_targets_per_anchor_ = 5;
    config.freq_bin_limit_ = 512;
    const HashGenerator gen(config);

    std::vector<Peak> peaks = {
        {0.0F, 0, 100},   // валидный якорь
        {0.0F, 2, 600},   // отброшен (bin >= 512)
        {0.0F, 5, 200},   // валидная цель
    };

    const auto fps = gen.Generate(peaks);
    ASSERT_EQ(fps.size(), 1U);

    const auto unpacked = HashGenerator::UnpackHash(fps[0].hash_);
    EXPECT_EQ(unpacked.freq_anchor, 100U);
    EXPECT_EQ(unpacked.freq_target, 200U);
    EXPECT_EQ(unpacked.time_delta, 5U);
}

// --- Лимит целей на якорь ------------------------------------------------

TEST(HashGeneratorTest, TargetLimitRespected) {
    HashGeneratorConfig config;
    config.max_target_offset_ = 100;
    config.max_targets_per_anchor_ = 2;
    const HashGenerator gen(config);

    // Один якорь (frame 0), четыре цели в пределах окна.
    std::vector<Peak> peaks = {
        {0.0F, 0, 100},
        {0.0F, 1, 110},
        {0.0F, 2, 120},
        {0.0F, 3, 130},
        {0.0F, 4, 140},
    };

    const auto fps = gen.Generate(peaks);

    // Якорь frame=0 -> макс 2 цели (frame 1 и frame 2).
    // Якорь frame=1 -> макс 2 цели (frame 2 и frame 3).
    // Якорь frame=2 -> макс 2 цели (frame 3 и frame 4).
    // Якорь frame=3 -> 1 цель (frame 4).
    // Якорь frame=4 -> 0 целей.
    // Итого: 2 + 2 + 2 + 1 = 7.
    EXPECT_EQ(fps.size(), 7U);

    // Проверяем, что первый якорь спарился именно с двумя ближайшими.
    const auto u0 = HashGenerator::UnpackHash(fps[0].hash_);
    const auto u1 = HashGenerator::UnpackHash(fps[1].hash_);
    EXPECT_EQ(u0.time_delta, 1U);
    EXPECT_EQ(u1.time_delta, 2U);
}

// --- Окно смещения -------------------------------------------------------

TEST(HashGeneratorTest, TargetBeyondOffsetWindowIsSkipped) {
    HashGeneratorConfig config;
    config.max_target_offset_ = 5;
    config.max_targets_per_anchor_ = 10;
    const HashGenerator gen(config);

    std::vector<Peak> peaks = {
        {0.0F, 0, 100},
        {0.0F, 6, 200},   // delta=6 > max_target_offset_=5
    };

    const auto fps = gen.Generate(peaks);
    EXPECT_TRUE(fps.empty());
}

TEST(HashGeneratorTest, TargetAtExactOffsetBoundaryIsIncluded) {
    HashGeneratorConfig config;
    config.max_target_offset_ = 5;
    config.max_targets_per_anchor_ = 10;
    const HashGenerator gen(config);

    std::vector<Peak> peaks = {
        {0.0F, 0, 100},
        {0.0F, 5, 200},   // delta=5 == max_target_offset_ -> включён
    };

    const auto fps = gen.Generate(peaks);
    ASSERT_EQ(fps.size(), 1U);
}

// --- Пустой вход ---------------------------------------------------------

TEST(HashGeneratorTest, EmptyPeaksGiveEmptyFingerprints) {
    const HashGenerator gen;
    const auto fps = gen.Generate({});
    EXPECT_TRUE(fps.empty());
}

TEST(HashGeneratorTest, SinglePeakGivesEmptyFingerprints) {
    const HashGenerator gen;
    std::vector<Peak> peaks = {{0.0F, 5, 100}};
    const auto fps = gen.Generate(peaks);
    EXPECT_TRUE(fps.empty());
}

}  // namespace
}  // namespace aid::core