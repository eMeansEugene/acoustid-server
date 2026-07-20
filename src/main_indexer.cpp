//
// CLI-инструмент для офлайн-индексирования треков.
// Использование: ./indexer --input <path> --title "..." --artist "..." [--config config.json]
//

#include <iostream>
#include <string>

#include "app_config.h"
#include "audio/audio_decoder.h"
#include "core/audio_fingerprint_engine.h"
#include "domain/indexing_service.h"
#include "storage/sqlite_repository.h"

void PrintUsage(const char* program) {
    std::cerr << "Usage: " << program
              << " --input <path> --title <title> --artist <artist>"
              << " [--config config.json] [--db <path>]\n";
}

int main(int argc, char* argv[]) {
    std::string input_path;
    std::string title;
    std::string artist;
    std::string config_path = "config.json";
    std::string db_override;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) input_path = argv[++i];
        else if (arg == "--title" && i + 1 < argc) title = argv[++i];
        else if (arg == "--artist" && i + 1 < argc) artist = argv[++i];
        else if (arg == "--config" && i + 1 < argc) config_path = argv[++i];
        else if (arg == "--db" && i + 1 < argc) db_override = argv[++i];
        else if (arg == "--help" || arg == "-h") { PrintUsage(argv[0]); return 0; }
    }

    if (input_path.empty() || title.empty() || artist.empty()) {
        PrintUsage(argv[0]);
        return 1;
    }

    // --- Загрузка конфига ---
    aid::AppConfig config;
    try {
        config = aid::AppConfig::LoadFromFile(config_path);
    } catch (const std::exception&) {
        config = aid::AppConfig::Defaults();
    }

    // --db переопределяет конфиг.
    if (!db_override.empty()) {
        config.db_path = db_override;
    }

    try {
        aid::storage::SQLiteRepository repository(config.db_path);
        aid::audio::AudioDecoder decoder;
        aid::core::AudioFingerprintEngine engine(config.fft, config.peak, config.hash);
        aid::domain::IndexingService indexing(decoder, engine, repository);

        std::cout << "Indexing: " << input_path << "\n";
        const auto result = indexing.IndexFromFile(input_path, {title, artist, 0.0F});

        std::cout << "Done: track_id=" << result.track_id
                  << " fingerprints=" << result.fingerprint_count << "\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}