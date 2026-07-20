//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_AUDIO_FINGERPRINT_ENGINE_H
#define ACOUSTID_SERVER_AUDIO_FINGERPRINT_ENGINE_H
#include <vector>

#include "core/fft_engine.h"
#include "core/hash_generator.h"
#include "core/peak_extractor.h"

namespace aid::core {

    /// Все результаты DSP-пайплайна: от спектрограммы до fingerprints.
    /// Промежуточные данные (spectrogram, peaks) нужны для визуализации.
    struct FingerprintResult {
        Spectrogram spectrogram;
        std::vector<Peak> peaks;
        std::vector<Fingerprint> fingerprints;
    };

    /// Фасад DSP-пайплайна: samples → spectrogram → peaks → fingerprints.
    ///
    /// Не знает про аудиоформаты (MP3/WAV) — принимает уже декодированные
    /// float-сэмплы. Декодирование — ответственность вызывающего кода.
    class AudioFingerprintEngine {
    public:
        AudioFingerprintEngine(FftEngineConfig fft_config = {}, const PeakExtractorConfig& peak_config = {},
                               const HashGeneratorConfig& hash_config = {});

        /// Прогоняет полный пайплайн и возвращает все промежуточные результаты.
        FingerprintResult Process(const std::vector<float>& samples) const;

    private:
        FftEngine fft_;
        PeakExtractor peak_extractor_;
        HashGenerator hash_generator_;
    };

}  // namespace aid::core
#endif // ACOUSTID_SERVER_AUDIO_FINGERPRINT_ENGINE_H
