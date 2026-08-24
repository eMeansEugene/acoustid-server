//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_SERVER_SERVER_CONFIG_H
#define ACOUSTID_SERVER_SERVER_SERVER_CONFIG_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace aid::server {

    /// Конфигурация HTTP-сервера. Не зависит от Crow —
    /// может использоваться в AppConfig без подтягивания HTTP-библиотеки.
    struct HttpServerConfig {
        uint16_t port = 8080;                              ///< TCP-порт, на котором слушает сервер.
        std::string admin_api_key = "changeme";             ///< Ключ авторизации для админ-эндпоинтов (индексирование, удаление).
        std::size_t max_upload_bytes = 50 * 1024 * 1024;   ///< Максимальный размер загружаемого файла, байт (50 МБ).
        bool debug_save_audio = false;                      ///< Сохранять входящие аудиофайлы в debug_audio_dir.
        std::string debug_audio_dir = "debug_audio";        ///< Каталог для отладочных копий входящих файлов.
    };

}  // namespace aid::server
#endif // ACOUSTID_SERVER_SERVER_SERVER_CONFIG_H
