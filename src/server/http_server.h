//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_HTTP_SERVER_H
#define ACOUSTID_SERVER_HTTP_SERVER_H


#include <string>

#include <crow.h>
#include "server_config.h"
#include "domain/i_track_repository.h"
#include "domain/indexing_service.h"
#include "server/task_queue.h"
#include "server/task_registry.h"

namespace aid::server {


    /// HTTP-сервер на Crow: маршрутизация, обработка запросов, авторизация.
    class HttpServer {
    public:
        HttpServer(HttpServerConfig config,
                   TaskQueue& queue,
                   TaskRegistry& registry,
                   domain::IndexingService& indexing,
                   domain::ITrackRepository& repository);

        /// Запустить сервер (блокирующий вызов).
        void Run();

        /// Остановить сервер.
        void Stop();

    private:
        HttpServerConfig config_;
        TaskQueue& queue_;
        TaskRegistry& registry_;
        domain::IndexingService& indexing_;
        domain::ITrackRepository& repository_;
        crow::SimpleApp app_;

        void SetupRoutes();

        /// Генерирует случайный hex-идентификатор задачи (12 символов).
        static std::string GenerateTaskId();

        // --- Handlers ---
        crow::response HandleMatch(const crow::request& req);
        crow::response HandleGetTask(const std::string& task_id);
        crow::response HandleGetTracks() const;
        crow::response HandleAdminIndex(const crow::request& req) const;
    };

}  // namespace aid::server

#endif // ACOUSTID_SERVER_HTTP_SERVER_H
