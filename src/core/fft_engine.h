//
// Created by evgen on 08.07.2026.
//

#ifndef ACOUSTID_SERVER_FFT_ENGINE_H
#define ACOUSTID_SERVER_FFT_ENGINE_H

#pragma once

#include <cstddef>
#include <vector>

namespace aid::core {

/// Параметры оконного FFT-пайплайна.
struct FftEngineConfig {
    /// Размер фрейма в сэмплах. Должен быть чётным (используется r2c FFT).
    std::size_t frame_size_ = 2048;

    /// Шаг между началами соседних фреймов, в сэмплах.
    /// hop_size = frame_size / 2 соответствует перекрытию 50%.
    std::size_t hop_size_ = 1024;
};

/// Спектрограмма — матрица (число фреймов) x (число бинов) значений
/// логарифмической мощности, в дБ (10 * log10(power + eps)).
///
/// Хранение row-major: элемент (frame, bin) находится по индексу
/// frame * num_bins + bin.
class Spectrogram {
public:
    Spectrogram(std::size_t num_frames, std::size_t num_bins);

    float& At(std::size_t frame_index, std::size_t bin_index);
    float At(std::size_t frame_index, std::size_t bin_index) const;

    std::size_t NumFrames() const noexcept { return num_frames_; }
    std::size_t NumBins() const noexcept { return num_bins_; }

    /// Доступ к сырому буферу (для сериализации в JSON построчно).
    const std::vector<float>& RawData() const noexcept { return data_; }

private:
    std::size_t num_frames_;
    std::size_t num_bins_;
    std::vector<float> data_;
};

/// Вычисляет спектрограмму сигнала.
///
/// Пайплайн на каждый фрейм: вырезка сэмплов -> оконное сглаживание
/// (Hann) -> вещественное FFT (pocketfft r2c) -> логарифмическая мощность.
///
/// Из frame_size вещественных сэмплов r2c-преобразование даёт
/// frame_size / 2 + 1 комплексных бинов; нулевой (DC) и последний
/// (Найквист) бины отбрасываются, остаётся frame_size / 2 бинов —
/// см. NumBins().
class FftEngine {
public:
    explicit FftEngine(FftEngineConfig config = {});

    /// Разбивает сигнал на перекрывающиеся фреймы и строит спектрограмму.
    /// Если samples короче одного фрейма, возвращается спектрограмма
    /// с нулём фреймов.
    Spectrogram ComputeSpectrogram(const std::vector<float>& samples) const;

    std::size_t NumBins() const noexcept { return config_.frame_size_ / 2; }
    const FftEngineConfig& Config() const noexcept { return config_; }

private:
    FftEngineConfig config_;
    std::vector<float> hann_window_;

    static std::vector<float> MakeHannWindow(std::size_t size);

    /// Обрабатывает один фрейм: window -> r2c FFT -> log-power в out.
    void ProcessFrame(const float* frame_samples, float* out_row) const;
};

}  // namespace aid::core

#endif // ACOUSTID_SERVER_FFT_ENGINE_H
