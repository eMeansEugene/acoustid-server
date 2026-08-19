#include "voting_engine.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <vector>

namespace aid::core {

namespace {

struct VoteKey {
    std::size_t track_id;
    int64_t delta;

    bool operator==(const VoteKey& other) const {
        return track_id == other.track_id && delta == other.delta;
    }
};

struct VoteKeyHash {
    std::size_t operator()(const VoteKey& key) const {
        std::size_t h1 = std::hash<std::size_t>{}(key.track_id);
        std::size_t h2 = std::hash<int64_t>{}(key.delta);
        return h1 ^ (h2 * 2654435761U);
    }
};

}  // namespace

VotingEngine::VotingEngine(VotingEngineConfig config) : config_(config) {}

std::optional<MatchResult> VotingEngine::Vote(const std::vector<HashMatch>& matches) const {
    if (matches.empty()) {
        return std::nullopt;
    }

    // Подсчёт голосов по (track_id, Δ).
    std::unordered_map<VoteKey, std::size_t, VoteKeyHash> votes;
    for (const HashMatch& match : matches) {
        const auto delta = static_cast<int64_t>(match.track_anchor_frame_) -
                            static_cast<int64_t>(match.fragment_anchor_frame_);
        votes[{match.track_id_, delta}]++;
    }

    // Найти первое и второе место.
    VoteKey best_key{};
    std::size_t best_votes = 0;
    std::size_t runner_up_votes = 0;

    for (const auto& [key, count] : votes) {
        if (count > best_votes) {
            runner_up_votes = best_votes;
            best_votes = count;
            best_key = key;
        } else if (count > runner_up_votes) {
            runner_up_votes = count;
        }
    }

    // Debug: top-5 кандидатов по голосам.
    {
        std::vector<std::pair<VoteKey, std::size_t>> sorted_votes(votes.begin(), votes.end());
        std::partial_sort(sorted_votes.begin(),
                          sorted_votes.begin() + std::min<std::size_t>(5, sorted_votes.size()),
                          sorted_votes.end(),
                          [](const auto& a, const auto& b) { return a.second > b.second; });
        std::cerr << "[voting] " << votes.size() << " unique (track,delta) pairs. Top candidates:\n";
        for (std::size_t i = 0; i < std::min<std::size_t>(5, sorted_votes.size()); ++i) {
            const auto& [key, count] = sorted_votes[i];
            std::cerr << "  #" << (i+1) << " track=" << key.track_id
                      << " delta=" << key.delta << " votes=" << count << "\n";
        }
    }

    // Условие 1: абсолютный минимум голосов.
    if (best_votes < config_.min_votes_) {
        return std::nullopt;
    }

    // Условие 2: отрыв от второго кандидата.
    const double score = (runner_up_votes == 0)
                              ? std::numeric_limits<double>::infinity()
                              : static_cast<double>(best_votes) / static_cast<double>(runner_up_votes);

    if (score < config_.min_score_ratio_) {
        return std::nullopt;
    }

    return MatchResult{
        .track_id_ = best_key.track_id,
        .offset_frames_ = best_key.delta,
        .votes_ = best_votes,
        .runner_up_ = runner_up_votes,
        .score_ = score,
    };
}

}  // namespace aid::core