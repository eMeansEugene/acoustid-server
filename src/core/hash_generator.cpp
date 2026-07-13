//
// HashGenerator: constellation map -> fingerprints.
//

#include "hash_generator.h"

#include <algorithm>
#include <stdexcept>

namespace aid::core {

namespace {
constexpr std::size_t FREQ_BITS = 9;
constexpr std::size_t DELTA_BITS = 14;
constexpr std::size_t FREQ_MASK = (1U << FREQ_BITS) - 1;   // 0x1FF = 511
constexpr std::size_t DELTA_MASK = (1U << DELTA_BITS) - 1;  // 0x3FFF = 16383
}  // namespace

HashGenerator::HashGenerator(const HashGeneratorConfig& config) : config_(config) {
    if (config_.max_targets_per_anchor_ == 0) {
        throw std::invalid_argument("max_targets_per_anchor_ must be positive");
    }
    if (config_.freq_bin_limit_ == 0 || config_.freq_bin_limit_ > (1U << FREQ_BITS)) {
        throw std::invalid_argument("freq_bin_limit_ must be in [1, 512]");
    }
}

uint32_t HashGenerator::PackHash(const std::size_t freq_anchor, const std::size_t freq_target,
                                  const std::size_t time_delta) {
    //  [31 .. 23]  [22 .. 14]  [13 .. 0]
    //  freq_anchor freq_target time_delta
    return static_cast<uint32_t>(((freq_anchor & FREQ_MASK) << (FREQ_BITS + DELTA_BITS)) |
                                  ((freq_target & FREQ_MASK) << DELTA_BITS) |
                                  (time_delta & DELTA_MASK));
}

HashGenerator::UnpackedHash HashGenerator::UnpackHash(const uint32_t hash) {
    return {
        .freq_anchor = (hash >> (FREQ_BITS + DELTA_BITS)) & FREQ_MASK,
        .freq_target = (hash >> DELTA_BITS) & FREQ_MASK,
        .time_delta = hash & DELTA_MASK,
    };
}

std::vector<Fingerprint> HashGenerator::Generate(const std::vector<Peak>& peaks) const {
    // Отфильтровать пики с bin_index >= freq_bin_limit_ (верхние частоты).
    std::vector<const Peak*> valid;
    valid.reserve(peaks.size());
    for (const Peak& peak : peaks) {
        if (peak.bin_index_ < config_.freq_bin_limit_) {
            valid.push_back(&peak);
        }
    }

    std::vector<Fingerprint> fingerprints;

    for (std::size_t i = 0; i < valid.size(); ++i) {
        const Peak& anchor = *valid[i];
        std::size_t targets_found = 0;

        // Пики отсортированы по (frame_index, bin_index) на выходе из
        // PeakExtractor. Цели ищем начиная со следующего пика — тот же
        // фрейм тоже допустим (две ноты одновременно).
        for (std::size_t j = i + 1; j < valid.size() && targets_found < config_.max_targets_per_anchor_; ++j) {
            const Peak& target = *valid[j];

            const std::size_t delta = target.frame_index_ - anchor.frame_index_;

            // За пределами окна — дальше все пики ещё дальше по времени,
            // можно прервать.
            if (delta > config_.max_target_offset_) {
                break;
            }

            const uint32_t hash = PackHash(anchor.bin_index_, target.bin_index_, delta);
            fingerprints.push_back(Fingerprint{hash, anchor.frame_index_});
            ++targets_found;
        }
    }

    return fingerprints;
}

}  // namespace aid::core