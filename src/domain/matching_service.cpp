#include "matching_service.h"

#include <unordered_map>

namespace aid::domain {

MatchingService::MatchingService(const audio::AudioDecoder& decoder,
                                   const core::AudioFingerprintEngine& engine,
                                   ITrackRepository& repository,
                                   const core::VotingEngine& voter)
    : decoder_(decoder), engine_(engine), repository_(repository), voter_(voter) {}

MatchOutput MatchingService::Match(const std::vector<uint8_t>& bytes) {
    // 1. Декодировать фрагмент.
    const audio::AudioData audio = decoder_.DecodeFromBytes(bytes);

    // 2. DSP-пайплайн: сэмплы → спектрограмма → пики → fingerprints.
    core::FingerprintResult fp_result = engine_.Process(audio.samples_);

    // 3. Собрать хэши для запроса в БД.
    std::vector<uint32_t> hashes;
    hashes.reserve(fp_result.fingerprints.size());
    for (const core::Fingerprint& fp : fp_result.fingerprints) {
        hashes.push_back(fp.hash_);
    }

    // 4. Найти совпадения в БД.
    const std::vector<HashLookupResult> lookup_results = repository_.FindMatches(hashes);

    // 5. Соединить результаты БД с данными фрагмента.
    const std::vector<core::HashMatch> matches = BuildHashMatches(fp_result.fingerprints, lookup_results);

    // 6. Голосование.
    const std::optional<core::MatchResult> match = voter_.Vote(matches);

    // 7. Диагностика.
    MatchDiagnostics diag;
    diag.sample_rate = audio.sample_rate_;
    diag.duration_sec = audio.duration_sec_;
    diag.num_frames = fp_result.spectrogram.NumFrames();
    diag.num_peaks = fp_result.peaks.size();
    diag.num_fingerprints = fp_result.fingerprints.size();
    diag.num_db_matches = lookup_results.size();
    diag.num_hash_matches = matches.size();

    return MatchOutput{
        .fingerprint_result = std::move(fp_result),
        .match_result = match,
        .diagnostics = diag,
    };
}

std::vector<core::HashMatch> MatchingService::BuildHashMatches(
    const std::vector<core::Fingerprint>& fragment_fps,
    const std::vector<HashLookupResult>& lookup_results) {
    // Построить индекс: hash → список anchor_frame во фрагменте.
    // Один хэш может встречаться в нескольких позициях фрагмента.
    std::unordered_map<uint32_t, std::vector<std::size_t>> hash_to_fragment_frames;
    for (const core::Fingerprint& fp : fragment_fps) {
        hash_to_fragment_frames[fp.hash_].push_back(fp.anchor_frame_);
    }

    // Для каждого совпадения из БД — найти все позиции этого хэша во фрагменте
    // и создать HashMatch для каждой пары (трек-позиция, фрагмент-позиция).
    std::vector<core::HashMatch> matches;
    for (const HashLookupResult& lr : lookup_results) {
        const auto it = hash_to_fragment_frames.find(lr.hash_);
        if (it == hash_to_fragment_frames.end()) {
            continue;  // не должно случиться, но на всякий случай
        }
        for (const std::size_t fragment_frame : it->second) {
            matches.push_back(core::HashMatch{
                .track_id_ = lr.track_id_,
                .track_anchor_frame_ = lr.track_anchor_frame_,
                .fragment_anchor_frame_ = fragment_frame,
            });
        }
    }

    return matches;
}

}  // namespace aid::domain