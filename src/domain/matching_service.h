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

    struct MatchDiagnostics {
        std::size_t sample_rate = 0;
        float duration_sec = 0.0F;
        std::size_t num_frames = 0;
        std::size_t num_peaks = 0;
        std::size_t num_fingerprints = 0;
        std::size_t num_unique_hashes = 0; ///< Уникальных хэшей (после дедупликации).
        std::size_t num_db_matches = 0;    ///< Совпадений хэшей в БД.
        std::size_t num_hash_matches = 0;  ///< HashMatch после join.
    };

    /// Полный результат матчинга: DSP-данные (для визуализации) + результат голосования.
    struct MatchOutput {
        core::FingerprintResult fingerprint_result;    ///< Спектрограмма, пики, fingerprints.
        std::optional<core::MatchResult> match_result; ///< Результат голосования или nullopt.
        MatchDiagnostics diagnostics;                  ///< Статистика пайплайна.
    };

    /// Оркестрирует матчинг фрагмента: декодирование → DSP → поиск в БД → голосование.
    class MatchingService {
    public:
        MatchingService(const audio::AudioDecoder& decoder, const core::AudioFingerprintEngine& engine,
                        ITrackRepository& repository, const core::VotingEngine& voter);

        /// Выполнить матчинг фрагмента из байтов в памяти.
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
