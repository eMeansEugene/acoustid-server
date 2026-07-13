//
// VotingEngine: определение трека по совпавшим хэшам.
//

#include "voting_engine.h"

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace aid::core {

namespace {

/// Ключ группировки: (track_id, Δ).
struct VoteKey {
    std::size_t track_id;
    int64_t delta;

    bool operator==(const VoteKey& other) const {
        return track_id == other.track_id && delta == other.delta;
    }
};

struct VoteKeyHash {
    std::size_t operator()(const VoteKey& key) const {
        // Простой комбинатор — порядок бит не критичен, коллизии
        // влияют только на скорость подсчёта, не на корректность.
        const std::size_t h1 = std::hash<std::size_t>{}(key.track_id);
        const std::size_t h2 = std::hash<int64_t>{}(key.delta);
        return h1 ^ (h2 * 2654435761U);
    }
};

}  // namespace

VotingEngine::VotingEngine(const VotingEngineConfig config) : config_(config) {
    if (config_.min_confidence_ < 0.0 || config_.min_confidence_ > 1.0) {
        throw std::invalid_argument("min_confidence_ must be in [0.0, 1.0]");
    }
}

std::optional<MatchResult> VotingEngine::Vote(const std::vector<HashMatch>& matches,
                                               const std::size_t total_hashes) const {
    if (matches.empty() || total_hashes == 0) {
        return std::nullopt;
    }

    // Подсчёт голосов по (track_id, Δ).
    std::unordered_map<VoteKey, std::size_t, VoteKeyHash> votes;
    for (const auto& [track_id_, track_anchor_frame_, fragment_anchor_frame_] : matches) {
        const auto delta = static_cast<int64_t>(track_anchor_frame_) -
                            static_cast<int64_t>(fragment_anchor_frame_);
        votes[{track_id_, delta}]++;
    }

    // Поиск победителя.
    VoteKey best_key{};
    std::size_t best_votes = 0;
    for (const auto& [key, count] : votes) {
        if (count > best_votes) {
            best_votes = count;
            best_key = key;
        }
    }

    const double confidence = static_cast<double>(best_votes) / static_cast<double>(total_hashes);

    if (confidence < config_.min_confidence_) {
        return std::nullopt;
    }

    return MatchResult{
        .track_id_ = best_key.track_id,
        .offset_frames_ = best_key.delta,
        .confidence_ = confidence,
        .votes_ = best_votes,
    };
}

}  // namespace aid::core