//
// Created by evgen on 08.07.2026.
//

#include "fft_engine.h"

#include <cmath>
#include <complex>
#include <stdexcept>

#include "pocketfft_hdronly.h"

namespace aid::core {

namespace {
constexpr float LOG_POWER_EPSILON = 1e-10F;
}  // namespace

Spectrogram::Spectrogram(const std::size_t num_frames, const std::size_t num_bins)
    : num_frames_(num_frames), num_bins_(num_bins), data_(num_frames * num_bins, 0.0F) {}

float& Spectrogram::At(const std::size_t frame_index, const std::size_t bin_index) {
    return data_[frame_index * num_bins_ + bin_index];
}

float Spectrogram::At(const std::size_t frame_index, const std::size_t bin_index) const {
    return data_[frame_index * num_bins_ + bin_index];
}

FftEngine::FftEngine(const FftEngineConfig config) : config_(config) {
    if (config_.frame_size_ == 0 || config_.frame_size_ % 2 != 0) {
        throw std::invalid_argument("frame_size must be positive and even");
    }
    if (config_.hop_size_ == 0) {
        throw std::invalid_argument("hop_size must be positive");
    }
    hann_window_ = MakeHannWindow(config_.frame_size_);
}

std::vector<float> FftEngine::MakeHannWindow(std::size_t size) {
    std::vector<float> window(size);
    // periodic Hann: w[n] = 0.5 * (1 - cos(2*pi*n / N)), n in [0, N)
    // используем N (не N-1) в знаменателе — периодическое окно, стандарт
    const auto n = static_cast<float>(size);
    for (std::size_t i = 0; i < size; ++i) {
        window[i] = 0.5F * (1.0F - std::cos(2.0F * static_cast<float>(M_PI) * static_cast<float>(i) / n));
    }
    return window;
}

void FftEngine::ProcessFrame(const float* frame_samples, float* out_row) const {
    const std::size_t frame_size = config_.frame_size_;
    const std::size_t num_complex_bins = frame_size / 2 + 1;  // r2c output size

    std::vector<float> windowed(frame_size);
    for (std::size_t i = 0; i < frame_size; ++i) {
        windowed[i] = frame_samples[i] * hann_window_[i];
    }

    std::vector<std::complex<float>> spectrum(num_complex_bins);

    const pocketfft::shape_t shape{frame_size};
    const pocketfft::stride_t stride_in{sizeof(float)};
    const pocketfft::stride_t stride_out{sizeof(std::complex<float>)};

    pocketfft::r2c<float>(shape, stride_in, stride_out, /*axis=*/0, /*forward=*/true, windowed.data(),
                           spectrum.data(), /*fct=*/1.0F);

    // Бин 0 (DC) и бин frame_size/2 (Найквист) отбрасываются: они не несут
    // информации о гармониках сигнала и не нужны для constellation map.
    for (std::size_t bin = 0; bin < frame_size / 2; ++bin) {
        const std::complex<float> c = spectrum[bin + 1];
        const float power = c.real() * c.real() + c.imag() * c.imag();
        out_row[bin] = 10.0F * std::log10(power + LOG_POWER_EPSILON);
    }
}

Spectrogram FftEngine::ComputeSpectrogram(const std::vector<float>& samples) const {
    const std::size_t frame_size = config_.frame_size_;
    const std::size_t hop_size = config_.hop_size_;
    const std::size_t num_bins = NumBins();

    if (samples.size() < frame_size) {
        return Spectrogram(0, num_bins);
    }

    const std::size_t num_frames = (samples.size() - frame_size) / hop_size + 1;
    Spectrogram spectrogram(num_frames, num_bins);

    for (std::size_t frame = 0; frame < num_frames; ++frame) {
        const float* frame_start = samples.data() + frame * hop_size;
        ProcessFrame(frame_start, &spectrogram.At(frame, 0));
    }

    return spectrogram;
}

}  // namespace aid::core