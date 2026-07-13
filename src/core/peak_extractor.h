#pragma once

#include <cstddef>
#include <vector>

#include "fft_engine.h"

namespace aid::core {

/// Один локальный максимум спектрограммы (constellation point).
struct Peak {
    float p_max_;              ///< Мощность в точке пика, дБ.
    std::size_t frame_index_;  ///< Индекс фрейма (ось времени в Spectrogram).
    std::size_t bin_index_;    ///< Индекс частотного бина (ось частоты в Spectrogram).
};

/// Параметры выделения пиков.
struct PeakExtractorConfig {
    /// Радиус окна локального максимума по оси фреймов: полное окно (2*N+1).
    /// N=2 -> окно 5 по времени.
    std::size_t frame_radius_ = 2;

    /// Радиус окна локального максимума по оси бинов: полное окно (2*N+1).
    /// N=2 -> окно 5 по частоте.
    std::size_t bin_radius_ = 2;

    /// Отступ порога от медианы фрейма, дБ. Точка проходит отбор, если
    /// её мощность >= медиана(фрейма) + offset_db_.
    float offset_db_ = 6.0F;

    /// Размер зоны в фреймах для контроля плотности пиков.
    std::size_t zone_frames_ = 0;  // TODO: подобрать в тестах

    /// Максимум пиков на одну зону.
    std::size_t peak_limit_ = 50;
};

/// Извлекает constellation map (локальные максимумы) из спектрограммы.
class PeakExtractor {
public:
    explicit PeakExtractor(const PeakExtractorConfig &config = {});

    /// Находит локальные максимумы спектрограммы, отбирает по порогу
    /// (медиана фрейма + offset_db_) и ограничивает плотность (peak_limit_
    /// пиков на зону из zone_frames_ фреймов).
    std::vector<Peak> ExtractPeaks(const Spectrogram& spectrogram) const;

private:
    PeakExtractorConfig config_;

    /// Медиана мощности по всем бинам одного фрейма (порог = медиана + offset_db_).
    static float ComputeFrameMedian(const Spectrogram& spectrogram, std::size_t frame);

    /// true, если точка строго больше всех соседей в окне
    /// (2*frame_radius_+1) x (2*bin_radius_+1) вокруг неё.
    bool IsLocalMax(const Spectrogram& spectrogram, std::size_t frame, std::size_t bin) const;

    /// Группирует кандидатов по зонам (frame_index / zone_frames_) и в каждой
    /// зоне оставляет top peak_limit_ по p_max_. Последняя (неполная) зона
    /// обрабатывается так же, без особого случая.
    std::vector<Peak> ApplyDensityControl(const std::vector<Peak>& candidates) const;
};

}  // namespace aid::core