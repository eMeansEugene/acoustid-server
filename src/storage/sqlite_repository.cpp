//
// SQLiteRepository: ITrackRepository backed by SQLite.
//

#include "sqlite_repository.h"

#include <chrono>
#include <stdexcept>
#include <string>

namespace aid::storage {

namespace {

/// RAII-обёртка для sqlite3_stmt: финализирует при выходе из скоупа.
class StmtGuard {
public:
    explicit StmtGuard(sqlite3_stmt* stmt) : stmt_(stmt) {}
    ~StmtGuard() {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
    }
    StmtGuard(const StmtGuard&) = delete;
    StmtGuard& operator=(const StmtGuard&) = delete;

    sqlite3_stmt* Get() const { return stmt_; }

private:
    sqlite3_stmt* stmt_;
};

/// Текущий unix timestamp (секунды).
std::int64_t NowUnixSeconds() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

}  // namespace

// --- Конструктор / деструктор ---------------------------------------------

SQLiteRepository::SQLiteRepository(const std::string& db_path) {
    const int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        const std::string msg = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Failed to open SQLite database: " + msg);
    }

    EnableWalMode();
    CreateSchema();
}

SQLiteRepository::~SQLiteRepository() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void SQLiteRepository::EnableWalMode() {
    Execute("PRAGMA journal_mode=WAL;");
}

void SQLiteRepository::CreateSchema() {
    Execute(R"(
        CREATE TABLE IF NOT EXISTS tracks (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            title       TEXT    NOT NULL,
            artist      TEXT    NOT NULL,
            duration    REAL    NOT NULL,
            indexed_at  INTEGER NOT NULL
        );
    )");

    Execute(R"(
        CREATE TABLE IF NOT EXISTS fingerprints (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            track_id        INTEGER NOT NULL,
            hash            INTEGER NOT NULL,
            time_offset     INTEGER NOT NULL,
            FOREIGN KEY (track_id) REFERENCES tracks(id) ON DELETE CASCADE
        );
    )");

    Execute("CREATE INDEX IF NOT EXISTS idx_fingerprints_hash ON fingerprints(hash);");
    Execute("CREATE INDEX IF NOT EXISTS idx_fingerprints_track_id ON fingerprints(track_id);");

    // Включаем поддержку foreign keys (для каскадного удаления).
    Execute("PRAGMA foreign_keys=ON;");
}

void SQLiteRepository::Execute(const char* sql) {
    char* err_msg = nullptr;
    const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        const std::string msg = err_msg ? err_msg : "unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("SQLite exec failed: " + msg);
    }
}

// --- AddTrackWithFingerprints ---------------------------------------------

std::size_t SQLiteRepository::AddTrackWithFingerprints(const domain::TrackMetadata& metadata,
                                                        const std::vector<core::Fingerprint>& fingerprints) {
    std::lock_guard lock(mutex_);

    Execute("BEGIN TRANSACTION;");

    try {
        // 1. Вставить трек.
        sqlite3_stmt* raw_stmt = nullptr;
        int rc = sqlite3_prepare_v2(
            db_, "INSERT INTO tracks (title, artist, duration, indexed_at) VALUES (?, ?, ?, ?);", -1, &raw_stmt,
            nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare INSERT tracks: " + std::string(sqlite3_errmsg(db_)));
        }
        StmtGuard track_stmt(raw_stmt);

        sqlite3_bind_text(track_stmt.Get(), 1, metadata.title_.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(track_stmt.Get(), 2, metadata.artist_.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(track_stmt.Get(), 3, static_cast<double>(metadata.duration_sec_));
        sqlite3_bind_int64(track_stmt.Get(), 4, NowUnixSeconds());

        rc = sqlite3_step(track_stmt.Get());
        if (rc != SQLITE_DONE) {
            throw std::runtime_error("Failed to insert track: " + std::string(sqlite3_errmsg(db_)));
        }

        const auto track_id = static_cast<std::size_t>(sqlite3_last_insert_rowid(db_));

        // 2. Вставить fingerprints батчом.
        rc = sqlite3_prepare_v2(
            db_, "INSERT INTO fingerprints (track_id, hash, time_offset) VALUES (?, ?, ?);", -1, &raw_stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare INSERT fingerprints: " + std::string(sqlite3_errmsg(db_)));
        }
        StmtGuard fp_stmt(raw_stmt);

        for (const core::Fingerprint& fp : fingerprints) {
            sqlite3_reset(fp_stmt.Get());
            sqlite3_bind_int64(fp_stmt.Get(), 1, static_cast<sqlite3_int64>(track_id));
            sqlite3_bind_int64(fp_stmt.Get(), 2, static_cast<sqlite3_int64>(fp.hash_));
            sqlite3_bind_int64(fp_stmt.Get(), 3, static_cast<sqlite3_int64>(fp.anchor_frame_));

            rc = sqlite3_step(fp_stmt.Get());
            if (rc != SQLITE_DONE) {
                throw std::runtime_error("Failed to insert fingerprint: " + std::string(sqlite3_errmsg(db_)));
            }
        }

        Execute("COMMIT;");
        return track_id;

    } catch (...) {
        Execute("ROLLBACK;");
        throw;
    }
}

// --- FindMatches ----------------------------------------------------------

std::vector<domain::HashLookupResult> SQLiteRepository::FindMatches(const std::vector<uint32_t>& hashes) {
    std::lock_guard lock(mutex_);

    std::vector<domain::HashLookupResult> results;

    sqlite3_stmt* raw_stmt = nullptr;
    const int rc = sqlite3_prepare_v2(
        db_, "SELECT hash, track_id, time_offset FROM fingerprints WHERE hash = ?;", -1, &raw_stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare SELECT fingerprints: " + std::string(sqlite3_errmsg(db_)));
    }
    StmtGuard stmt(raw_stmt);

    for (const uint32_t hash : hashes) {
        sqlite3_reset(stmt.Get());
        sqlite3_bind_int64(stmt.Get(), 1, static_cast<sqlite3_int64>(hash));

        while (sqlite3_step(stmt.Get()) == SQLITE_ROW) {
            results.push_back(domain::HashLookupResult{
                .hash_ = static_cast<uint32_t>(sqlite3_column_int64(stmt.Get(), 0)),
                .track_id_ = static_cast<std::size_t>(sqlite3_column_int64(stmt.Get(), 1)),
                .track_anchor_frame_ = static_cast<std::size_t>(sqlite3_column_int64(stmt.Get(), 2)),
            });
        }
    }

    return results;
}

// --- GetAllTracks ---------------------------------------------------------

std::vector<domain::TrackInfo> SQLiteRepository::GetAllTracks() {
    std::lock_guard lock(mutex_);

    std::vector<domain::TrackInfo> tracks;

    sqlite3_stmt* raw_stmt = nullptr;
    const int rc = sqlite3_prepare_v2(
        db_, "SELECT id, title, artist, duration, indexed_at FROM tracks ORDER BY id;", -1, &raw_stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare SELECT tracks: " + std::string(sqlite3_errmsg(db_)));
    }
    StmtGuard stmt(raw_stmt);

    while (sqlite3_step(stmt.Get()) == SQLITE_ROW) {
        tracks.push_back(domain::TrackInfo{
            .id_ = static_cast<std::size_t>(sqlite3_column_int64(stmt.Get(), 0)),
            .title_ = reinterpret_cast<const char*>(sqlite3_column_text(stmt.Get(), 1)),
            .artist_ = reinterpret_cast<const char*>(sqlite3_column_text(stmt.Get(), 2)),
            .duration_sec_ = static_cast<float>(sqlite3_column_double(stmt.Get(), 3)),
            .indexed_at_ = sqlite3_column_int64(stmt.Get(), 4),
        });
    }

    return tracks;
}

// --- DeleteTrack ----------------------------------------------------------

void SQLiteRepository::DeleteTrack(const std::size_t track_id) {
    std::lock_guard lock(mutex_);

    sqlite3_stmt* raw_stmt = nullptr;
    const int rc = sqlite3_prepare_v2(db_, "DELETE FROM tracks WHERE id = ?;", -1, &raw_stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare DELETE tracks: " + std::string(sqlite3_errmsg(db_)));
    }
    StmtGuard stmt(raw_stmt);

    sqlite3_bind_int64(stmt.Get(), 1, static_cast<sqlite3_int64>(track_id));

    if (sqlite3_step(stmt.Get()) != SQLITE_DONE) {
        throw std::runtime_error("Failed to delete track: " + std::string(sqlite3_errmsg(db_)));
    }

    // Fingerprints удаляются каскадно через FOREIGN KEY ON DELETE CASCADE.
}

}  // namespace aid::storage