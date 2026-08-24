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

    // Этап 1: для каждого трека — его лучшая (track, delta) пара.
    struct TrackBest {
        int64_t best_delta = 0;
        std::size_t best_votes = 0;
    };
    std::unordered_map<std::size_t, TrackBest> per_track;

    for (const auto& [key, count] : votes) {
        auto& tb = per_track[key.track_id];
        if (count > tb.best_votes) {
            tb.best_votes = count;
            tb.best_delta = key.delta;
        }
    }

    // Этап 2: сравниваем между треками, не между (track, delta) парами.
    std::size_t best_track = 0;
    int64_t best_delta = 0;
    std::size_t best_votes = 0;
    std::size_t runner_up_votes = 0;

    for (const auto& [track_id, tb] : per_track) {
        if (tb.best_votes > best_votes) {
            runner_up_votes = best_votes;
            best_votes = tb.best_votes;
            best_track = track_id;
            best_delta = tb.best_delta;
        } else if (tb.best_votes > runner_up_votes) {
            runner_up_votes = tb.best_votes;
        }
    }

    // Debug: top-5 треков по лучшим голосам.
    {
        std::vector<std::pair<std::size_t, TrackBest>> sorted_tracks(per_track.begin(), per_track.end());
        std::partial_sort(sorted_tracks.begin(),
                          sorted_tracks.begin() + std::min<std::size_t>(5, sorted_tracks.size()),
                          sorted_tracks.end(),
                          [](const auto& a, const auto& b) { return a.second.best_votes > b.second.best_votes; });
        std::cerr << "[voting] " << per_track.size() << " candidate tracks. Top:\n";
        for (std::size_t i = 0; i < std::min<std::size_t>(5, sorted_tracks.size()); ++i) {
            const auto& [tid, tb] = sorted_tracks[i];
            std::cerr << "  #" << (i+1) << " track=" << tid
                      << " best_delta=" << tb.best_delta
                      << " votes=" << tb.best_votes << "\n";
        }
    }

    // Условие 1: абсолютный минимум голосов.
    if (best_votes < config_.min_votes_) {
        return std::nullopt;
    }

    // Условие 2: отрыв от лучшего ДРУГОГО трека.
    const double score = (runner_up_votes == 0)
                              ? std::numeric_limits<double>::infinity()
                              : static_cast<double>(best_votes) / static_cast<double>(runner_up_votes);

    if (score < config_.min_score_ratio_) {
        return std::nullopt;
    }

    return MatchResult{
        .track_id_ = best_track,
        .offset_frames_ = best_delta,
        .votes_ = best_votes,
        .runner_up_ = runner_up_votes,
        .score_ = score,
    };
}

}  // namespace aid::core