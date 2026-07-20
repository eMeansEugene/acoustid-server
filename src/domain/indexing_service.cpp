//
// Created by evgen on 20.07.2026.
//

#include "indexing_service.h"

namespace aid::domain {

    IndexingService::IndexingService(const audio::AudioDecoder& decoder,
                                       const core::AudioFingerprintEngine& engine,
                                       ITrackRepository& repository)
        : decoder_(decoder), engine_(engine), repository_(repository) {}

    IndexingResult IndexingService::IndexFromBytes(const std::vector<uint8_t>& bytes,
                                                     const TrackMetadata& metadata) {
        const audio::AudioData audio = decoder_.DecodeFromBytes(bytes);
        return DoIndex(audio, metadata);
    }

    IndexingResult IndexingService::IndexFromFile(const std::string& path,
                                                    const TrackMetadata& metadata) {
        const audio::AudioData audio = decoder_.DecodeFromFile(path);
        return DoIndex(audio, metadata);
    }

    IndexingResult IndexingService::DoIndex(const audio::AudioData& audio,
                                              const TrackMetadata& metadata) {
        // Если duration не задан явно — берём из декодированного аудио.
        TrackMetadata effective = metadata;
        if (effective.duration_sec_ <= 0.0F) {
            effective.duration_sec_ = audio.duration_sec_;
        }

        const core::FingerprintResult result = engine_.Process(audio.samples_);

        const std::size_t track_id = repository_.AddTrackWithFingerprints(effective, result.fingerprints);

        return IndexingResult{track_id, result.fingerprints.size()};
    }

}  // namespace aid::domain
