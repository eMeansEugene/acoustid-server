#ifndef ACOUSTID_SERVER_CORE_PEAK_EXTRACTOR_H
#define ACOUSTID_SERVER_CORE_PEAK_EXTRACTOR_H

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
    std::size_t frame_radius_ = 2;

    /// Радиус окна локального максимума по оси бинов: полное окно (2*N+1).
    std::size_t bin_radius_ = 2;

    /// Отступ порога от медианы фрейма, дБ.
    float offset_db_ = 6.0F;

    /// Размер зоны в фреймах для контроля плотности пиков.
    std::size_t zone_frames_ = 0;

    /// Максимум пиков на одну частотную полосу в одной зоне.
    /// Итого: peaks_per_band_ × число полос пиков на зону.
    std::size_t peaks_per_band_ = 4;
};

/// Границы частотных полос (в бинах). Логарифмическое разбиение,
/// соответствует музыкально значимым диапазонам при frame_size=2048, sr=44100.
/// Каждая пара — [begin, end) бинов.
struct FrequencyBand {
    std::size_t begin;  ///< Первый бин (включительно).
    std::size_t end;    ///< Последний бин (не включая).
};

/// @brief Возвращает фиксированный список частотных полос, используемых
/// PeakExtractor для контроля плотности пиков по спектру.
/// @return Ссылка на статический вектор из 8 полос (суб-бас … воздух),
/// см. значения в реализации.
inline const std::vector<FrequencyBand>& GetFrequencyBands() {
    static const std::vector<FrequencyBand> bands = {
        {1,    4},     // ~21–86 Гц      суб-бас
        {4,    8},     // ~86–172 Гц      бас
        {8,    16},    // ~172–344 Гц     низ-середина
        {16,   32},    // ~344–688 Гц     середина
        {32,   64},    // ~688–1377 Гц    верх-середина
        {64,   128},   // ~1377–2754 Гц   присутствие
        {128,  256},   // ~2754–5511 Гц   яркость
        {256,  512},   // ~5511–11025 Гц  воздух
    };
    return bands;
}

/// Извлекает constellation map (локальные максимумы) из спектрограммы.
class PeakExtractor {
public:
    /// @brief Создаёт экстрактор пиков с заданными параметрами.
    /// @param config Параметры выделения пиков (копируется во внутреннее поле).
    /// @throws std::invalid_argument если config.zone_frames_ == 0.
    explicit PeakExtractor(PeakExtractorConfig config = {});

    /// @brief Находит локальные максимумы спектрограммы, отбирает по порогу
    /// (медиана фрейма + offset_db_) и ограничивает плотность (не более
    /// peaks_per_band_ пиков на каждую частотную полосу в каждой зоне из
    /// zone_frames_ фреймов).
    /// @param spectrogram Спектрограмма источника (см. FftEngine).
    /// @return Отфильтрованный список пиков в детерминированном порядке
    /// (по frame_index_, затем bin_index_).
    std::vector<Peak> ExtractPeaks(const Spectrogram& spectrogram) const;

private:
    PeakExtractorConfig config_;

    /// Медиана мощности по всем бинам одного фрейма (порог = медиана + offset_db_).
    float ComputeFrameMedian(const Spectrogram& spectrogram, std::size_t frame) const;

    /// true, если точка строго больше всех соседей в окне
    /// (2*frame_radius_+1) x (2*bin_radius_+1) вокруг неё.
    bool IsLocalMax(const Spectrogram& spectrogram, std::size_t frame, std::size_t bin) const;

    /// Группирует кандидатов по (зона, частотная полоса), где зона —
    /// frame_index_ / zone_frames_, а полоса — одна из GetFrequencyBands().
    /// В каждой группе оставляет top peaks_per_band_ по p_max_. Последняя
    /// (неполная) зона обрабатывается так же, без особого случая.
    std::vector<Peak> ApplyDensityControl(std::vector<Peak> candidates) const;
};

}  // namespace aid::core

#endif  // ACOUSTID_SERVER_CORE_PEAK_EXTRACTOR_H
