#ifndef ACOUSTID_SERVER_CORE_HASH_GENERATOR_H
#define ACOUSTID_SERVER_CORE_HASH_GENERATOR_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "peak_extractor.h"

namespace aid::core {

/// Один отпечаток: хэш пары пиков + абсолютное время якоря (для голосования).
struct Fingerprint {
    uint32_t hash_;              ///< Упакованный хэш: freq_anchor(9) | freq_target(9) | time_delta(14).
    std::size_t anchor_frame_;   ///< Индекс фрейма якоря (не входит в хэш).
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
    /// @param config Параметры генерации (окно целей, лимиты).
    explicit HashGenerator(const HashGeneratorConfig& config = {});

    /// @brief Принимает отсортированный по (frame_index, bin_index) список пиков
    /// (выход PeakExtractor). Возвращает список fingerprints.
    /// @param peaks Constellation map, отсортированная по (frame_index_, bin_index_).
    /// @return Список fingerprints (может быть пустым, если пиков мало).
    std::vector<Fingerprint> Generate(const std::vector<Peak>& peaks) const;

    /// @brief Упаковать три компоненты в uint32_t.
    /// @param freq_anchor Частотный бин якоря (9 бит).
    /// @param freq_target Частотный бин цели (9 бит).
    /// @param time_delta Разница фреймов между якорем и целью (14 бит).
    /// @return Упакованный хэш.
    static uint32_t PackHash(std::size_t freq_anchor, std::size_t freq_target, std::size_t time_delta);

    /// Результат распаковки хэша обратно в компоненты.
    struct UnpackedHash {
        std::size_t freq_anchor;  ///< Частотный бин якоря.
        std::size_t freq_target;  ///< Частотный бин цели.
        std::size_t time_delta;   ///< Разница фреймов между якорем и целью.
    };

    /// @brief Распаковать uint32_t обратно в три компоненты.
    /// @param hash Упакованный хэш (см. PackHash).
    /// @return Распакованные freq_anchor/freq_target/time_delta.
    static UnpackedHash UnpackHash(uint32_t hash);

private:
    HashGeneratorConfig config_;
};

}  // namespace aid::core

#endif  // ACOUSTID_SERVER_CORE_HASH_GENERATOR_H