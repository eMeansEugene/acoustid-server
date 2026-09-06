//
// Тесты HTTP-обработчиков через Crow handle_full() — без сети, без потоков.
//

#include "server/http_server.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include "audio/audio_decoder.h"
#include "core/audio_fingerprint_engine.h"
#include "core/voting_engine.h"
#include "domain/indexing_service.h"
#include "domain/matching_service.h"
#include "storage/sqlite_repository.h"

namespace aid::server {
namespace {

using json = nlohmann::json;

std::vector<uint8_t> MakeTestWav() {
    const uint32_t sample_rate = 44100;
    const uint32_t num_samples = sample_rate;  // 1 сек
    const uint32_t data_size = num_samples * 2;

    std::vector<uint8_t> buf;
    buf.reserve(44 + data_size);

    auto write_u16 = [&](uint16_t v) { buf.push_back(v & 0xFF); buf.push_back((v >> 8) & 0xFF); };
    auto write_u32 = [&](uint32_t v) {
        buf.push_back(v & 0xFF); buf.push_back((v >> 8) & 0xFF);
        buf.push_back((v >> 16) & 0xFF); buf.push_back((v >> 24) & 0xFF);
    };
    auto write_tag = [&](const char* tag) { for (int i = 0; i < 4; i++) buf.push_back(tag[i]); };
    auto write_i16 = [&](int16_t v) { buf.push_back(v & 0xFF); buf.push_back((v >> 8) & 0xFF); };

    write_tag("RIFF"); write_u32(36 + data_size); write_tag("WAVE");
    write_tag("fmt "); write_u32(16);
    write_u16(1); write_u16(1);
    write_u32(sample_rate); write_u32(sample_rate * 2);
    write_u16(2); write_u16(16);
    write_tag("data"); write_u32(data_size);

    for (uint32_t i = 0; i < num_samples; i++) {
        auto v = static_cast<int16_t>(16000 * std::sin(2.0 * M_PI * 440.0 * i / sample_rate));
        write_i16(v);
    }
    return buf;
}

// --- Хелпер: собрать multipart body ---
std::string BuildMultipart(const std::string& boundary,
                            const std::vector<uint8_t>& file_data,
                            const std::string& filename,
                            const std::vector<std::pair<std::string, std::string>>& fields) {
    std::string body;
    // Текстовые поля
    for (const auto& [name, value] : fields) {
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"" + name + "\"\r\n\r\n";
        body += value + "\r\n";
    }
    // Файл
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
    body += "Content-Type: application/octet-stream\r\n\r\n";
    body.append(reinterpret_cast<const char*>(file_data.data()), file_data.size());
    body += "\r\n";
    body += "--" + boundary + "--\r\n";
    return body;
}

// --- Fixture: поднимает все компоненты без сети ---
class HttpHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        repo_ = std::make_unique<storage::SQLiteRepository>(":memory:");

        core::PeakExtractorConfig peak_config;
        peak_config.zone_frames_ = 43;
        peak_config.peaks_per_band_ = 50;
        engine_ = std::make_unique<core::AudioFingerprintEngine>(
            core::FftEngineConfig{}, peak_config, core::HashGeneratorConfig{});

        core::VotingEngineConfig vote_config;
        vote_config.min_votes_ = 1;
        vote_config.min_score_ratio_ = 1.0;
        voter_ = std::make_unique<core::VotingEngine>(vote_config);

        indexing_ = std::make_unique<domain::IndexingService>(decoder_, *engine_, *repo_);
        matching_ = std::make_unique<domain::MatchingService>(decoder_, *engine_, *repo_, *voter_);

        HttpServerConfig config;
        config.admin_api_key = "test-key";
        server_ = std::make_unique<HttpServer>(config, queue_, registry_, *indexing_, *repo_);
    }

    crow::response Handle(const std::string& method, const std::string& url,
                           const std::string& body = "",
                           const std::string& content_type = "",
                           const std::string& api_key = "") {
        crow::request req;
        req.url = url;
        req.raw_url = url;
        if (method == "POST") req.method = crow::HTTPMethod::POST;
        else req.method = crow::HTTPMethod::GET;
        req.body = body;
        if (!content_type.empty()) {
            req.add_header("Content-Type", content_type);
        }
        if (!api_key.empty()) {
            req.add_header("X-Api-Key", api_key);
        }
        crow::response res;
        server_->Handle(req, res);
        return res;
    }

    crow::response PostMultipart(const std::string& url,
                                  const std::vector<uint8_t>& file_data,
                                  const std::string& filename,
                                  const std::vector<std::pair<std::string, std::string>>& fields = {},
                                  const std::string& api_key = "") {
        const std::string boundary = "----TestBoundary12345";
        std::string body = BuildMultipart(boundary, file_data, filename, fields);
        return Handle("POST", url, body,
                      "multipart/form-data; boundary=" + boundary, api_key);
    }

    audio::AudioDecoder decoder_;
    std::unique_ptr<storage::SQLiteRepository> repo_;
    std::unique_ptr<core::AudioFingerprintEngine> engine_;
    std::unique_ptr<core::VotingEngine> voter_;
    std::unique_ptr<domain::IndexingService> indexing_;
    std::unique_ptr<domain::MatchingService> matching_;
    TaskQueue queue_;
    TaskRegistry registry_;
    std::unique_ptr<HttpServer> server_;
};

// === GET /tracks — пустая база ===

TEST_F(HttpHandlerTest, GetTracksEmptyReturns200) {
    auto res = Handle("GET", "/tracks");
    EXPECT_EQ(res.code, 200);
    auto j = json::parse(res.body);
    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(j.size(), 0U);
}

// === POST /admin/index — авторизация ===

TEST_F(HttpHandlerTest, AdminIndexNoKeyReturns401) {
    auto wav = MakeTestWav();
    auto res = PostMultipart("/admin/index", wav, "test.wav",
                              {{"title", "T"}, {"artist", "A"}});
    EXPECT_EQ(res.code, 401);
}

TEST_F(HttpHandlerTest, AdminIndexWrongKeyReturns401) {
    auto wav = MakeTestWav();
    auto res = PostMultipart("/admin/index", wav, "test.wav",
                              {{"title", "T"}, {"artist", "A"}}, "wrong-key");
    EXPECT_EQ(res.code, 401);
}

// === POST /admin/index — успешная индексация ===

TEST_F(HttpHandlerTest, AdminIndexValidReturns200) {
    auto wav = MakeTestWav();
    auto res = PostMultipart("/admin/index", wav, "test.wav",
                              {{"title", "Test Song"}, {"artist", "Test Artist"}}, "test-key");
    EXPECT_EQ(res.code, 200);
    auto j = json::parse(res.body);
    EXPECT_TRUE(j.contains("track_id"));
    EXPECT_TRUE(j.contains("fingerprint_count"));
    EXPECT_GT(j["fingerprint_count"].get<int>(), 0);
}

// === POST /admin/index — без обязательных полей ===

TEST_F(HttpHandlerTest, AdminIndexNoTitleReturns400) {
    auto wav = MakeTestWav();
    auto res = PostMultipart("/admin/index", wav, "test.wav",
                              {{"artist", "A"}}, "test-key");
    EXPECT_EQ(res.code, 400);
}

// === GET /tracks — после индексации ===

TEST_F(HttpHandlerTest, GetTracksAfterIndexReturnsTrack) {
    auto wav = MakeTestWav();
    PostMultipart("/admin/index", wav, "test.wav",
                  {{"title", "My Song"}, {"artist", "My Artist"}}, "test-key");

    auto res = Handle("GET", "/tracks");
    EXPECT_EQ(res.code, 200);
    auto j = json::parse(res.body);
    ASSERT_EQ(j.size(), 1U);
    EXPECT_EQ(j[0]["title"], "My Song");
    EXPECT_EQ(j[0]["artist"], "My Artist");
}

// === POST /match — возвращает 202 + task_id ===

TEST_F(HttpHandlerTest, MatchReturns202WithTaskId) {
    auto wav = MakeTestWav();
    auto res = PostMultipart("/match", wav, "fragment.wav");
    EXPECT_EQ(res.code, 202);
    auto j = json::parse(res.body);
    EXPECT_TRUE(j.contains("task_id"));
    EXPECT_FALSE(j["task_id"].get<std::string>().empty());
}

// === GET /tasks/{id} — задача зарегистрирована ===

TEST_F(HttpHandlerTest, GetTaskReturnsPending) {
    auto wav = MakeTestWav();
    auto match_res = PostMultipart("/match", wav, "fragment.wav");
    auto task_id = json::parse(match_res.body)["task_id"].get<std::string>();

    auto res = Handle("GET", "/tasks/" + task_id);
    EXPECT_EQ(res.code, 200);
    auto j = json::parse(res.body);
    EXPECT_EQ(j["task_id"], task_id);
    // Статус pending или processing (воркер не запущен в тестах).
    std::string status = j["status"];
    EXPECT_TRUE(status == "pending" || status == "processing");
}

// === GET /tasks/{unknown} — 404 ===

TEST_F(HttpHandlerTest, GetTaskUnknownReturns404) {
    auto res = Handle("GET", "/tasks/nonexistent-id-12345");
    EXPECT_EQ(res.code, 404);
}

}  // namespace
}  // namespace aid::server
