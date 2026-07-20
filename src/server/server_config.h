//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_SERVER_CONFIG_H
#define ACOUSTID_SERVER_SERVER_CONFIG_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace aid::server {

    /// Конфигурация HTTP-сервера. Не зависит от Crow —
    /// может использоваться в AppConfig без подтягивания HTTP-библиотеки.
    struct HttpServerConfig {
        uint16_t port = 8080;
        std::string admin_api_key = "changeme";
        std::size_t max_upload_bytes = 50 * 1024 * 1024;  ///< 50 МБ.
    };

}  // namespace aid::server
#endif // ACOUSTID_SERVER_SERVER_CONFIG_H
