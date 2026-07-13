//
// Created by evgen on 13.07.2026.
//

#ifndef ACOUSTID_SERVER_VOTING_ENGINE_H
#define ACOUSTID_SERVER_VOTING_ENGINE_H

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace aid::core {

    /// Одно совпадение хэша фрагмента с хэшем из базы данных.
    struct HashMatch {
        std::size_t track_id_;
        std::size_t track_anchor_frame_;     ///< Позиция якоря в треке (из БД).
        std::size_t fragment_anchor_frame_;  ///< Позиция якоря во фрагменте.
    };

    /// Результат голосования: идентифицированный трек.
    struct MatchResult {
        std::size_t track_id_;
        int64_t offset_frames_;  ///< Δ = track_anchor - fragment_anchor (смещение фрагмента в треке).
        double confidence_;      ///< Доля голосов победителя от общего числа хэшей фрагмента.
        std::size_t votes_;      ///< Абсолютное число голосов победителя.
    };

    /// Параметры голосования.
    struct VotingEngineConfig {
        /// Минимальный confidence для признания совпадения.
        /// Ниже порога — возвращается nullopt (NoMatch).
        double min_confidence_ = 0.1;
    };

    /// Определяет трек и позицию фрагмента по набору совпавших хэшей.
    ///
    /// Для каждого совпадения вычисляет Δ = track_anchor - fragment_anchor.
    /// Группирует по (track_id, Δ) и подсчитывает голоса. Пара с максимумом
    /// голосов — победитель. Если confidence ниже порога — NoMatch.
    class VotingEngine {
    public:
        explicit VotingEngine(VotingEngineConfig config = {});

        /// @param matches      Совпавшие хэши (из репозитория).
        /// @param total_hashes Общее число хэшей фрагмента (для расчёта confidence).
        /// @return Результат матчинга или nullopt, если confidence ниже порога.
        std::optional<MatchResult> Vote(const std::vector<HashMatch>& matches,
                                        std::size_t total_hashes) const;

    private:
        VotingEngineConfig config_;
    };

}  // namespace aid::core

#endif // ACOUSTID_SERVER_VOTING_ENGINE_H
