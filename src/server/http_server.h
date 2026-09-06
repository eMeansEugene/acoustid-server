#ifndef ACOUSTID_SERVER_SERVER_HTTP_SERVER
#define ACOUSTID_SERVER_SERVER_HTTP_SERVER

#include <string>

#include <crow.h>

#include "domain/i_track_repository.h"
#include "domain/indexing_service.h"
#include "server/server_config.h"
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

    /// Обработать запрос напрямую, минуя сеть (для тестирования).
    void Handle(crow::request& req, crow::response& res);

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
    crow::response HandleGetTracks();
    crow::response HandleAdminIndex(const crow::request& req);
};

}  // namespace aid::server
#endif