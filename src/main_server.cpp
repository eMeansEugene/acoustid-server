//
// Точка входа HTTP-сервера.
// Использование: ./acoustid_server [--config config.json]
//

#include <iostream>
#include <string>

#include "app_config.h"
#include "audio/audio_decoder.h"
#include "core/audio_fingerprint_engine.h"
#include "core/voting_engine.h"
#include "domain/indexing_service.h"
#include "domain/matching_service.h"
#include "server/http_server.h"
#include "server/task_queue.h"
#include "server/task_registry.h"
#include "server/worker_pool.h"
#include "storage/sqlite_repository.h"

int main(int argc, char* argv[]) {
    // --- Загрузка конфига ---
    std::string config_path = "config.json";
    std::size_t num_workers = 4;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        }
    }

    aid::AppConfig config;
    try {
        config = aid::AppConfig::LoadFromFile(config_path);
        std::cout << "[main] Config loaded from " << config_path << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[main] Warning: " << e.what() << ", using defaults\n";
        config = aid::AppConfig::Defaults();
    }

    std::cout << "[main] DB: " << config.db_path << "\n";
    std::cout << "[main] Port: " << config.server.port << "\n";
    std::cout << "[main] Peak limit: " << config.peak.peak_limit_ << "\n";
    std::cout << "[main] Targets per anchor: " << config.hash.max_targets_per_anchor_ << "\n";
    std::cout << "[main] Offset dB: " << config.peak.offset_db_ << "\n";

    // --- Компоненты ---
    aid::storage::SQLiteRepository repository(config.db_path);
    aid::audio::AudioDecoder decoder;
    aid::core::AudioFingerprintEngine engine(config.fft, config.peak, config.hash);
    aid::core::VotingEngine voter(config.voting);

    aid::domain::IndexingService indexing(decoder, engine, repository);
    aid::domain::MatchingService matching(decoder, engine, repository, voter);

    // --- Сервер ---
    aid::server::TaskQueue queue;
    aid::server::TaskRegistry registry;
    aid::server::WorkerPool workers(num_workers, queue, registry, matching);
    aid::server::HttpServer server(config.server, queue, registry, indexing, repository);
    server.Run();

    return 0;
}