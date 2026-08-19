#include "http_server.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include <nlohmann/json.hpp>

namespace aid::server {

using json = nlohmann::json;

// --- Конструктор и управление ---------------------------------------------

HttpServer::HttpServer(HttpServerConfig config,
                         TaskQueue& queue,
                         TaskRegistry& registry,
                         domain::IndexingService& indexing,
                         domain::ITrackRepository& repository)
    : config_(std::move(config)),
      queue_(queue),
      registry_(registry),
      indexing_(indexing),
      repository_(repository) {
    SetupRoutes();
}

void HttpServer::Run() {
    std::cout << "[server] Starting on port " << config_.port << "\n";
    app_.port(config_.port).multithreaded().run();
}

void HttpServer::Stop() {
    app_.stop();
}

// --- Маршрутизация --------------------------------------------------------

void HttpServer::SetupRoutes() {
    CROW_ROUTE(app_, "/")([this]() {
        std::ifstream file("static/index.html");
        if (!file.is_open()) {
            return crow::response(404, "static/index.html not found");
        }
        std::string html((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
        auto resp = crow::response(200, html);
        resp.set_header("Content-Type", "text/html; charset=utf-8");
        return resp;
    });

    CROW_ROUTE(app_, "/match").methods("POST"_method)([this](const crow::request& req) {
        return HandleMatch(req);
    });

    CROW_ROUTE(app_, "/tasks/<string>")([this](const std::string& id) {
        return HandleGetTask(id);
    });

    CROW_ROUTE(app_, "/tracks")([this]() {
        return HandleGetTracks();
    });

    CROW_ROUTE(app_, "/admin/index").methods("POST"_method)([this](const crow::request& req) {
        return HandleAdminIndex(req);
    });
}

// --- POST /match ----------------------------------------------------------

crow::response HttpServer::HandleMatch(const crow::request& req) {
    crow::multipart::message msg(req);

    // Ищем part с файлом.
    const crow::multipart::part* file_part = nullptr;
    for (const auto& part : msg.parts) {
        auto it = part.headers.find("Content-Disposition");
        if (it != part.headers.end() && it->second.params.count("filename")) {
            file_part = &part;
            break;
        }
    }

    if (!file_part || file_part->body.empty()) {
        json err;
        err["error"] = "No audio file provided";
        return crow::response(400, err.dump());
    }

    if (file_part->body.size() > config_.max_upload_bytes) {
        json err;
        err["error"] = "File exceeds maximum size";
        return crow::response(413, err.dump());
    }

    // Создать задачу.
    const std::string task_id = GenerateTaskId();
    std::vector<uint8_t> bytes(file_part->body.begin(), file_part->body.end());

    // Debug: сохранить входящий файл на диск для прослушивания.
    if (config_.debug_save_audio) {
        std::filesystem::create_directories(config_.debug_audio_dir);
        const std::string debug_path = config_.debug_audio_dir + "/" + task_id + ".wav";
        std::ofstream out(debug_path, std::ios::binary);
        if (out.is_open()) {
            out.write(reinterpret_cast<const char*>(bytes.data()),
                       static_cast<std::streamsize>(bytes.size()));
            std::cout << "[debug] Saved audio: " << debug_path
                      << " (" << bytes.size() << " bytes)\n";
        }
    }

    registry_.Register(task_id);
    queue_.Push(Task{task_id, std::move(bytes)});

    std::cout << "[server] POST /match -> task_id=" << task_id << "\n";

    json body;
    body["task_id"] = task_id;
    auto resp = crow::response(202, body.dump());
    resp.set_header("Content-Type", "application/json");
    return resp;
}

// --- GET /tasks/{id} ------------------------------------------------------

crow::response HttpServer::HandleGetTask(const std::string& task_id) {
    const auto state = registry_.Get(task_id);
    if (!state.has_value()) {
        json err;
        err["error"] = "Task not found";
        return crow::response(404, err.dump());
    }

    json body;
    body["task_id"] = task_id;

    switch (state->status) {
        case TaskStatus::PENDING:
            body["status"] = "pending";
            break;
        case TaskStatus::PROCESSING:
            body["status"] = "processing";
            break;
        case TaskStatus::DONE:
            body["status"] = "done";
            if (state->output.has_value() && state->output->match_result.has_value()) {
                const auto& mr = *state->output->match_result;
                json result;
                result["track_id"] = mr.track_id_;
                result["offset_frames"] = mr.offset_frames_;
                result["votes"] = mr.votes_;
                result["runner_up"] = mr.runner_up_;
                result["score"] = mr.score_;

                const auto tracks = repository_.GetAllTracks();
                for (const auto& t : tracks) {
                    if (t.id_ == mr.track_id_) {
                        result["track"]["id"] = t.id_;
                        result["track"]["title"] = t.title_;
                        result["track"]["artist"] = t.artist_;
                        result["track"]["duration_sec"] = t.duration_sec_;
                        break;
                    }
                }

                body["result"] = result;
            } else {
                body["result"] = nullptr;
            }

            // Диагностика пайплайна — всегда, независимо от match/no-match.
            if (state->output.has_value()) {
                const auto& d = state->output->diagnostics;
                json diag;
                diag["sample_rate"] = d.sample_rate;
                diag["duration_sec"] = d.duration_sec;
                diag["num_frames"] = d.num_frames;
                diag["num_peaks"] = d.num_peaks;
                diag["num_fingerprints"] = d.num_fingerprints;
                diag["num_db_matches"] = d.num_db_matches;
                diag["num_hash_matches"] = d.num_hash_matches;
                body["diagnostics"] = diag;
            }
            break;
        case TaskStatus::ERROR:
            body["status"] = "error";
            body["error"] = state->error_message;
            break;
    }

    auto resp = crow::response(200, body.dump());
    resp.set_header("Content-Type", "application/json");
    return resp;
}

// --- GET /tracks ----------------------------------------------------------

crow::response HttpServer::HandleGetTracks() const  {
    const auto tracks = repository_.GetAllTracks();

    json body = json::array();
    for (const auto& t : tracks) {
        body.push_back({
            {"id", t.id_},
            {"title", t.title_},
            {"artist", t.artist_},
            {"duration_sec", t.duration_sec_},
            {"indexed_at", t.indexed_at_},
        });
    }

    auto resp = crow::response(200, body.dump());
    resp.set_header("Content-Type", "application/json");
    return resp;
}

// --- POST /admin/index ----------------------------------------------------

crow::response HttpServer::HandleAdminIndex(const crow::request& req) const {
    // Проверить API-ключ.
    const auto key = req.get_header_value("X-Api-Key");
    if (key != config_.admin_api_key) {
        json err;
        err["error"] = "Invalid or missing API key";
        return crow::response(401, err.dump());
    }

    crow::multipart::message msg(req);

    // Найти файл.
    const crow::multipart::part* file_part = nullptr;
    std::string title;
    std::string artist;

    for (const auto& part : msg.parts) {
        auto it = part.headers.find("Content-Disposition");
        if (it == part.headers.end()) continue;

        const auto& params = it->second.params;
        auto name_it = params.find("name");
        if (name_it == params.end()) continue;

        if (name_it->second == "file" || params.count("filename")) {
            file_part = &part;
        } else if (name_it->second == "title") {
            title = part.body;
        } else if (name_it->second == "artist") {
            artist = part.body;
        }
    }

    if (!file_part || file_part->body.empty()) {
        json err;
        err["error"] = "No audio file provided";
        return crow::response(400, err.dump());
    }
    if (title.empty()) {
        json err;
        err["error"] = "Missing required field: title";
        return crow::response(400, err.dump());
    }
    if (artist.empty()) {
        json err;
        err["error"] = "Missing required field: artist";
        return crow::response(400, err.dump());
    }

    try {
        std::vector<uint8_t> bytes(file_part->body.begin(), file_part->body.end());
        const auto result = indexing_.IndexFromBytes(bytes, {title, artist, 0.0F});

        std::cout << "[server] POST /admin/index -> track_id=" << result.track_id
                  << " fingerprints=" << result.fingerprint_count << "\n";

        json body;
        body["track_id"] = result.track_id;
        body["fingerprint_count"] = result.fingerprint_count;

        auto resp = crow::response(200, body.dump());
        resp.set_header("Content-Type", "application/json");
        return resp;

    } catch (const std::exception& e) {
        json err;
        err["error"] = std::string("Indexing failed: ") + e.what();
        return crow::response(400, err.dump());
    }
}

// --- Утилиты --------------------------------------------------------------

std::string HttpServer::GenerateTaskId() {
    static std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(12) << (dist(gen) & 0xFFFFFFFFFFFFULL);
    return oss.str();
}

}  // namespace aid::server