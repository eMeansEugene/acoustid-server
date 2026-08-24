//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_DOMAIN_INDEXING_SERVICE_H
#define ACOUSTID_SERVER_DOMAIN_INDEXING_SERVICE_H

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
        std::size_t track_id;            ///< Идентификатор созданного трека.
        std::size_t fingerprint_count;   ///< Количество сохранённых fingerprints.
    };

    /// Оркестрирует индексирование трека: декодирование → DSP → запись в БД.
    ///
    /// Используется из CLI и из AdminHandler без дублирования логики.
    class IndexingService {
    public:
        /// @param decoder Декодер аудио (MP3/WAV → float-сэмплы).
        /// @param engine DSP-пайплайн (сэмплы → fingerprints).
        /// @param repository Хранилище треков и fingerprints.
        IndexingService(const audio::AudioDecoder& decoder,
                         const core::AudioFingerprintEngine& engine,
                         ITrackRepository& repository);

        /// Проиндексировать трек из байтов в памяти (HTTP upload).
        /// @param bytes Содержимое аудиофайла.
        /// @param metadata Название, исполнитель, длительность.
        /// @return Идентификатор трека и число сохранённых fingerprints.
        /// @throws std::runtime_error при ошибке декодирования или записи в БД.
        IndexingResult IndexFromBytes(const std::vector<uint8_t>& bytes,
                                       const TrackMetadata& metadata);

        /// Проиндексировать трек из файла на диске (CLI).
        /// @param path Путь к аудиофайлу.
        /// @param metadata Название, исполнитель, длительность.
        /// @return Идентификатор трека и число сохранённых fingerprints.
        /// @throws std::runtime_error при ошибке чтения, декодирования или записи в БД.
        IndexingResult IndexFromFile(const std::string& path,
                                      const TrackMetadata& metadata);

    private:
        const audio::AudioDecoder& decoder_;
        const core::AudioFingerprintEngine& engine_;
        ITrackRepository& repository_;

        IndexingResult DoIndex(const audio::AudioData& audio, const TrackMetadata& metadata);
    };

}  // namespace aid::domain

#endif // ACOUSTID_SERVER_DOMAIN_INDEXING_SERVICE_H
