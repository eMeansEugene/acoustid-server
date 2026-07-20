//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_INDEXING_SERVICE_H
#define ACOUSTID_SERVER_INDEXING_SERVICE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "audio/audio_decoder.h"
#include "core/audio_fingerprint_engine.h"
#include "domain/i_track_repository.h"

namespace aid::domain {

    /// Результат индексирования (для ответа API).
    struct IndexingResult {
        std::size_t track_id;
        std::size_t fingerprint_count;
    };

    /// Оркестрирует индексирование трека: декодирование → DSP → запись в БД.
    ///
    /// Используется из CLI и из AdminHandler без дублирования логики.
    class IndexingService {
    public:
        IndexingService(const audio::AudioDecoder& decoder,
                         const core::AudioFingerprintEngine& engine,
                         ITrackRepository& repository);

        /// Проиндексировать трек из байтов в памяти (HTTP upload).
        IndexingResult IndexFromBytes(const std::vector<uint8_t>& bytes,
                                       const TrackMetadata& metadata);

        /// Проиндексировать трек из файла на диске (CLI).
        IndexingResult IndexFromFile(const std::string& path,
                                      const TrackMetadata& metadata);

    private:
        const audio::AudioDecoder& decoder_;
        const core::AudioFingerprintEngine& engine_;
        ITrackRepository& repository_;

        IndexingResult DoIndex(const audio::AudioData& audio, const TrackMetadata& metadata);
    };

}  // namespace aid::domain

#endif // ACOUSTID_SERVER_INDEXING_SERVICE_H
