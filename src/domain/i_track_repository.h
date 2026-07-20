//
// Created by evgen on 14.07.2026.
//

#ifndef ACOUSTID_SERVER_I_TRACK_REPOSITORY_H
#define ACOUSTID_SERVER_I_TRACK_REPOSITORY_H
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/hash_generator.h"

namespace aid::domain {

    /// Метаданные трека для индексирования (входные данные).
    struct TrackMetadata {
        std::string title_;
        std::string artist_;
        float duration_sec_ = 0.0F;
    };

    /// Полная информация о треке (для списка и API-ответов).
    struct TrackInfo {
        std::size_t id_;
        std::string title_;
        std::string artist_;
        float duration_sec_;
        std::int64_t indexed_at_;  ///< Unix timestamp (секунды).
    };

    /// Результат поиска хэша в базе данных.
    /// Не содержит информации о фрагменте — только о треке.
    /// MatchingService самостоятельно соединяет с данными фрагмента
    /// для построения HashMatch.
    struct HashLookupResult {
        uint32_t hash_;
        std::size_t track_id_;
        std::size_t track_anchor_frame_;  ///< Индекс фрейма якоря в треке.
    };

    /// Абстрактный интерфейс хранилища треков и fingerprints.
    ///
    /// Domain-слой определяет контракт; storage-слой реализует.
    /// Все методы потокобезопасны в реализации.
    class ITrackRepository {
    public:
        virtual ~ITrackRepository() = default;

        /// Атомарно сохраняет трек и все его fingerprints.
        /// @return Идентификатор созданного трека.
        /// @throws std::runtime_error при ошибке записи.
        virtual std::size_t AddTrackWithFingerprints(const TrackMetadata& metadata,
                                                      const std::vector<core::Fingerprint>& fingerprints) = 0;

        /// Ищет все совпадения для набора хэшей.
        /// @return Список совпадений (может быть пустым).
        virtual std::vector<HashLookupResult> FindMatches(const std::vector<uint32_t>& hashes) = 0;

        /// Возвращает список всех проиндексированных треков.
        virtual std::vector<TrackInfo> GetAllTracks() = 0;

        /// Удаляет трек и все его fingerprints.
        /// Если трек не найден — ничего не делает (идемпотентно).
        virtual void DeleteTrack(std::size_t track_id) = 0;
    };

}  // namespace aid::domain
#endif // ACOUSTID_SERVER_I_TRACK_REPOSITORY_H
