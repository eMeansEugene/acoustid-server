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
        std::string db_path = "tracks.db";  ///< Путь к файлу SQLite с базой треков.

        core::FftEngineConfig fft;          ///< Параметры оконного FFT (frame_size, hop_size).
        core::PeakExtractorConfig peak;     ///< Параметры выделения пиков (constellation map).
        core::HashGeneratorConfig hash;     ///< Параметры генерации fingerprint-хэшей.
        core::VotingEngineConfig voting;    ///< Пороги голосования (min_votes, min_score_ratio).
        server::HttpServerConfig server;    ///< Параметры HTTP-сервера (порт, ключ API).

        /// Загрузить из JSON-файла. Отсутствующие поля сохраняют дефолтные значения.
        /// @param path Путь к JSON-файлу конфигурации.
        /// @return Заполненный AppConfig.
        /// @throws std::runtime_error при ошибке чтения файла или невалидном JSON.
        static AppConfig LoadFromFile(const std::string& path);

        /// Дефолтный конфиг (без файла).
        /// @return AppConfig со значениями по умолчанию.
        static AppConfig Defaults();
    };

}  // namespace aid

#endif // ACOUSTID_SERVER_APP_CONFIG_H
