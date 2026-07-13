#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "peak_extractor.h"

namespace aid::core {

/// Один отпечаток: хэш пары пиков + абсолютное время якоря (для голосования).
struct Fingerprint {
    uint32_t hash_;           ///< Упакованный хэш: freq_anchor(9) | freq_target(9) | time_delta(14).
    std::size_t anchor_frame_;  ///< Индекс фрейма якоря (не входит в хэш).
};

/// Параметры генерации хэшей.
struct HashGeneratorConfig {
    /// Максимальное смещение цели относительно якоря по фреймам (включительно).
    /// Цель ищется в диапазоне [anchor_frame, anchor_frame + max_target_offset_].
    std::size_t max_target_offset_ = 100;

    /// Максимум целей на один якорь (берутся ближайшие по времени).
    std::size_t max_targets_per_anchor_ = 5;

    /// Верхняя граница bin_index (не включая). Пики с bin_index >= freq_bin_limit_
    /// отбрасываются. 512 = 9 бит, покрывает ~0–11 кГц при frame_size=2048 / 44.1 кГц.
    std::size_t freq_bin_limit_ = 512;
};

/// Генерирует fingerprints из constellation map.
///
/// Для каждого пика-якоря перебирает пики-цели в пределах временного окна
/// и упаковывает пару (freq_anchor, freq_target, time_delta) в uint32_t.
/// Пики с bin_index >= freq_bin_limit_ отбрасываются (верхние частоты).
class HashGenerator {
public:
    explicit HashGenerator(const HashGeneratorConfig& config = {});

    /// Принимает отсортированный по (frame_index, bin_index) список пиков
    /// (выход PeakExtractor). Возвращает список fingerprints.
    std::vector<Fingerprint> Generate(const std::vector<Peak>& peaks) const;

    /// Упаковать три компоненты в uint32_t.
    static uint32_t PackHash(std::size_t freq_anchor, std::size_t freq_target, std::size_t time_delta);

    /// Распаковать uint32_t обратно в три компоненты.
    struct UnpackedHash {
        std::size_t freq_anchor;
        std::size_t freq_target;
        std::size_t time_delta;
    };
    static UnpackedHash UnpackHash(uint32_t hash);

private:
    HashGeneratorConfig config_;
};

}  // namespace aid::core