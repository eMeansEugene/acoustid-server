//
// PeakExtractor: выделение constellation map из спектрограммы.
//

#include "peak_extractor.h"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <unordered_map>

namespace aid::core {

PeakExtractor::PeakExtractor(const PeakExtractorConfig& config) : config_(config) {
    if (config_.zone_frames_ == 0) {
        throw std::invalid_argument("zone_frames_ must be positive");
    }
}

float PeakExtractor::ComputeFrameMedian(const Spectrogram& spectrogram, const std::size_t frame) {
    const std::size_t num_bins = spectrogram.NumBins();
    std::vector<float> row(num_bins);
    for (std::size_t bin = 0; bin < num_bins; ++bin) {
        row[bin] = spectrogram.At(frame, bin);
    }

    const std::size_t mid = num_bins / 2;
    std::ranges::nth_element(row, row.begin() + static_cast<std::ptrdiff_t>(mid));
    const float upper = row[mid];

    if (num_bins % 2 == 1) {
        return upper;
    }

    // Чётное число бинов: медиана — среднее двух центральных элементов.
    std::nth_element(row.begin(), row.begin() + static_cast<std::ptrdiff_t>(mid - 1),
                      row.begin() + static_cast<std::ptrdiff_t>(mid));
    const float lower = row[mid - 1];
    return 0.5F * (upper + lower);
}

bool PeakExtractor::IsLocalMax(const Spectrogram& spectrogram, const std::size_t frame, const std::size_t bin) const {
    const float center = spectrogram.At(frame, bin);

    const std::size_t frame_from = frame - config_.frame_radius_;
    const std::size_t frame_to = frame + config_.frame_radius_;
    const std::size_t bin_from = bin - config_.bin_radius_;
    const std::size_t bin_to = bin + config_.bin_radius_;

    for (std::size_t f = frame_from; f <= frame_to; ++f) {
        for (std::size_t b = bin_from; b <= bin_to; ++b) {
            if (f == frame && b == bin) {
                continue;
            }
            // Строгое сравнение: точка должна быть выше ВСЕХ соседей.
            // Равенство соседу означает, что оба — не единственный максимум,
            // ни одна из точек-соседей с равным значением пиком не считается.
            if (spectrogram.At(f, b) >= center) {
                return false;
            }
        }
    }
    return true;
}

std::vector<Peak> PeakExtractor::ApplyDensityControl(const std::vector<Peak>& candidates) const {
    std::unordered_map<std::size_t, std::vector<Peak>> zones;
    for (const Peak& peak : candidates) {
        zones[peak.frame_index_ / config_.zone_frames_].push_back(peak);
    }

    std::vector<Peak> result;
    result.reserve(candidates.size());

    for (auto& zone_peaks : zones | std::views::values) {
        if (zone_peaks.size() > config_.peak_limit_) {
            std::ranges::partial_sort(zone_peaks, zone_peaks.begin() + static_cast<std::ptrdiff_t>(config_.peak_limit_),
                               [](const Peak& a, const Peak& b) { return a.p_max_ > b.p_max_; });
            zone_peaks.resize(config_.peak_limit_);
        }
        result.insert(result.end(), zone_peaks.begin(), zone_peaks.end());
    }

    // Детерминированный порядок на выходе — не зависит от порядка обхода
    // unordered_map. Порядок пиков сам по себе для матчинга не важен, но
    // важен для воспроизводимости тестов.
    std::ranges::sort(result, [](const Peak& a, const Peak& b) {
        if (a.frame_index_ != b.frame_index_) {
            return a.frame_index_ < b.frame_index_;
        }
        return a.bin_index_ < b.bin_index_;
    });

    return result;
}

std::vector<Peak> PeakExtractor::ExtractPeaks(const Spectrogram& spectrogram) const {
    const std::size_t num_frames = spectrogram.NumFrames();
    const std::size_t num_bins = spectrogram.NumBins();

    std::vector<Peak> candidates;

    // Краевые точки, для которых окно не помещается целиком, не
    // рассматриваются как кандидаты
    if (num_frames <= 2 * config_.frame_radius_ || num_bins <= 2 * config_.bin_radius_) {
        return candidates;
    }

    const std::size_t frame_begin = config_.frame_radius_;
    const std::size_t frame_end = num_frames - 1 - config_.frame_radius_;  // включительно
    const std::size_t bin_begin = config_.bin_radius_;
    const std::size_t bin_end = num_bins - 1 - config_.bin_radius_;  // включительно

    for (std::size_t frame = frame_begin; frame <= frame_end; ++frame) {
        const float threshold = ComputeFrameMedian(spectrogram, frame) + config_.offset_db_;

        for (std::size_t bin = bin_begin; bin <= bin_end; ++bin) {
            const float value = spectrogram.At(frame, bin);
            if (value < threshold) {
                continue;
            }
            if (!IsLocalMax(spectrogram, frame, bin)) {
                continue;
            }
            candidates.push_back(Peak{value, frame, bin});
        }
    }

    return ApplyDensityControl(std::move(candidates));
}

}  // namespace aid::core