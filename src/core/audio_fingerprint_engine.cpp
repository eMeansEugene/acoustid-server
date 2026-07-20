#include "audio_fingerprint_engine.h"

namespace aid::core {

    AudioFingerprintEngine::AudioFingerprintEngine(const FftEngineConfig fft_config,
                                                   const PeakExtractorConfig& peak_config,
                                                   const HashGeneratorConfig& hash_config)
        : fft_(fft_config), peak_extractor_(peak_config), hash_generator_(hash_config) {}

    FingerprintResult AudioFingerprintEngine::Process(const std::vector<float>& samples) const {
        Spectrogram spectrogram = fft_.ComputeSpectrogram(samples);
        std::vector<Peak> peaks = peak_extractor_.ExtractPeaks(spectrogram);
        std::vector<Fingerprint> fingerprints = hash_generator_.Generate(peaks);

        return FingerprintResult{
            .spectrogram = std::move(spectrogram),
            .peaks = std::move(peaks),
            .fingerprints = std::move(fingerprints),
        };
    }

}  // namespace aid::core