//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_DOMAIN_MATCHING_SERVICE_H
#define ACOUSTID_SERVER_DOMAIN_MATCHING_SERVICE_H

#include <cstdint>
#include <optional>
#include <vector>

#include "audio/audio_decoder.h"
#include "core/audio_fingerprint_engine.h"
#include "core/voting_engine.h"
#include "domain/i_track_repository.h"

namespace aid::domain {

    /// Диагностика пайплайна распознавания (для логов и отладки).
    struct MatchDiagnostics {
        std::size_t sample_rate = 0;        ///< Частота дискретизации фрагмента.
        float duration_sec = 0.0F;          ///< Длительность фрагмента, сек.
        std::size_t num_frames = 0;         ///< Число фреймов спектрограммы.
        std::size_t num_peaks = 0;          ///< Число извлечённых пиков.
        std::size_t num_fingerprints = 0;   ///< Число сгенерированных fingerprints.
        std::size_t num_unique_hashes = 0;  ///< Уникальных хэшей (после дедупликации).
        std::size_t num_db_matches = 0;     ///< Совпадений хэшей в БД.
        std::size_t num_hash_matches = 0;   ///< HashMatch после join.
    };

    /// Полный результат распознавания: DSP-данные (для визуализации) + результат голосования.
    struct MatchOutput {
        core::FingerprintResult fingerprint_result;    ///< Спектрограмма, пики, fingerprints.
        std::optional<core::MatchResult> match_result; ///< Результат голосования или nullopt.
        MatchDiagnostics diagnostics;                  ///< Статистика пайплайна.
    };

    /// Координирует распознавание фрагмента: декодирование → DSP → поиск в БД → голосование.
    class MatchingService {
    public:
        /// @param decoder Декодер аудио (MP3/WAV → float-сэмплы).
        /// @param engine DSP-пайплайн (сэмплы → fingerprints).
        /// @param repository Хранилище треков и fingerprints (для поиска совпадений).
        /// @param voter Механизм голосования (совпавшие хэши → трек-победитель).
        MatchingService(const audio::AudioDecoder& decoder, const core::AudioFingerprintEngine& engine,
                        ITrackRepository& repository, const core::VotingEngine& voter);

        /// Выполнить распознавание фрагмента из байтов в памяти.
        /// @param bytes Содержимое аудиофрагмента (MP3/WAV).
        /// @return DSP-данные, результат голосования (или nullopt) и диагностика.
        /// @throws std::runtime_error при ошибке декодирования.
        MatchOutput Match(const std::vector<uint8_t>& bytes);

    private:
        const audio::AudioDecoder& decoder_;
        const core::AudioFingerprintEngine& engine_;
        ITrackRepository& repository_;
        const core::VotingEngine& voter_;

        /// Соединяет HashLookupResult из БД с данными фрагмента → HashMatch для VotingEngine.
        static std::vector<core::HashMatch> BuildHashMatches(const std::vector<core::Fingerprint>& fragment_fps,
                                                             const std::vector<HashLookupResult>& lookup_results);
    };

} // namespace aid::domain
#endif // ACOUSTID_SERVER_DOMAIN_MATCHING_SERVICE_H
