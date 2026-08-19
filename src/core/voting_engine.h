//
// Created by evgen on 13.07.2026.
//

#ifndef ACOUSTID_SERVER_VOTING_ENGINE_H
#define ACOUSTID_SERVER_VOTING_ENGINE_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace aid::core {

    /// Одно совпадение хэша фрагмента с хэшем из базы данных.
    struct HashMatch {
        std::size_t track_id_;
        std::size_t track_anchor_frame_;    ///< Позиция якоря в треке (из БД).
        std::size_t fragment_anchor_frame_; ///< Позиция якоря во фрагменте.
    };

    /// Результат голосования: идентифицированный трек.
    struct MatchResult {
        std::size_t track_id_;
        int64_t offset_frames_; ///< Δ = track_anchor - fragment_anchor.
        std::size_t votes_;     ///< Голоса победителя (высота пика гистограммы).
        std::size_t runner_up_; ///< Голоса второго кандидата (0 если единственный).
        double score_;          ///< votes / runner_up (inf если runner_up == 0).
    };

    /// Параметры голосования.
    struct VotingEngineConfig {
        /// Минимальное абсолютное количество голосов.
        std::size_t min_votes_ = 10;

        /// Минимальное отношение победителя ко второму кандидату.
        double min_score_ratio_ = 2.0;
    };

    /// Определяет трек и позицию фрагмента по набору совпавших хэшей.
    class VotingEngine {
    public:
        explicit VotingEngine(VotingEngineConfig config = {});

        /// @param matches Совпавшие хэши (из репозитория).
        /// @return Результат или nullopt, если votes < min_votes_
        ///         или winner/runner_up < min_score_ratio_.
        std::optional<MatchResult> Vote(const std::vector<HashMatch>& matches) const;

    private:
        VotingEngineConfig config_;
    };

} // namespace aid::core
#endif // ACOUSTID_SERVER_VOTING_ENGINE_H
