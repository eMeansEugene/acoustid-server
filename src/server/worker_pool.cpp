#include "worker_pool.h"

#include <iostream>

namespace aid::server {

WorkerPool::WorkerPool(const std::size_t num_workers,
                         TaskQueue& queue,
                         TaskRegistry& registry,
                         domain::MatchingService& matcher)
    : queue_(queue), registry_(registry), matcher_(matcher) {
    workers_.reserve(num_workers);
    for (std::size_t i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&WorkerPool::WorkerLoop, this);
    }
}

WorkerPool::~WorkerPool() {
    queue_.Stop();
    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void WorkerPool::WorkerLoop() const {
    while (true) {
        auto task = queue_.Pop();
        if (!task.has_value()) {
            break;  // очередь остановлена
        }

        const std::string& task_id = task->id;
        registry_.SetProcessing(task_id);

        try {
            domain::MatchOutput output = matcher_.Match(task->audio_bytes);
            const auto& d = output.diagnostics;
            std::cout << "[worker] Task " << task_id
                      << " | sr=" << d.sample_rate
                      << " dur=" << d.duration_sec << "s"
                      << " frames=" << d.num_frames
                      << " peaks=" << d.num_peaks
                      << " fps=" << d.num_fingerprints
                      << " unique=" << d.num_unique_hashes
                      << " db_matches=" << d.num_db_matches
                      << " hash_matches=" << d.num_hash_matches;
            if (output.match_result) {
                std::cout << " | MATCH track=" << output.match_result->track_id_
                          << " votes=" << output.match_result->votes_
                          << " runner_up=" << output.match_result->runner_up_
                          << " score=" << output.match_result->score_;
            } else {
                std::cout << " | NO MATCH";
            }
            std::cout << "\n";

            // Debug: top-5 candidates по голосам
            if (d.num_hash_matches > 0) {
                // Пересчитать голоса для логирования (дёшево, уже посчитано в Vote).
                std::unordered_map<std::size_t, std::size_t> track_total_votes;
                // Грубая метрика: сколько hash_matches приходится на каждый track_id.
                // Не учитывает Δ, но показывает, какие треки вообще в игре.
                const auto& fps = output.fingerprint_result.fingerprints;
                // Используем diagnostics — просто логируем.
                std::cout << "[debug] Top candidates by total hash matches per track:\n";

                // Собираем track_id -> count из всех hash_matches.
                // Для этого нужен доступ к matches, но они не сохраняются.
                // Вместо этого выведем top по голосам из VotingEngine — для этого
                // нужно расширить Vote(). Пока выводим что есть.
                std::cout << "[debug] (enable detailed voting log in VotingEngine for per-delta breakdown)\n";
            }

            registry_.SetDone(task_id, std::move(output));
        } catch (const std::exception& e) {
            registry_.SetError(task_id, e.what());
            std::cerr << "[worker] Task " << task_id << " failed: " << e.what() << "\n";
        } catch (...) {
            registry_.SetError(task_id, "Unknown error");
            std::cerr << "[worker] Task " << task_id << " failed: unknown error\n";
        }
    }
}

}  // namespace aid::server