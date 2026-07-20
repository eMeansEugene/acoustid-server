//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_APP_CONFIG_H
#define ACOUSTID_SERVER_APP_CONFIG_H


#include <string>

#include "core/fft_engine.h"
#include "core/hash_generator.h"
#include "core/peak_extractor.h"
#include "core/voting_engine.h"
#include "server/server_config.h"

namespace aid {

    /// Все настройки приложения, загружаемые из JSON-файла.
    struct AppConfig {
        std::string db_path = "tracks.db";

        core::FftEngineConfig fft;
        core::PeakExtractorConfig peak;
        core::HashGeneratorConfig hash;
        core::VotingEngineConfig voting;
        server::HttpServerConfig server;

        /// Загрузить из JSON-файла. Отсутствующие поля сохраняют дефолтные значения.
        /// @throws std::runtime_error при ошибке чтения файла.
        static AppConfig LoadFromFile(const std::string& path);

        /// Дефолтный конфиг (без файла).
        static AppConfig Defaults();
    };

}  // namespace aid

#endif // ACOUSTID_SERVER_APP_CONFIG_H
