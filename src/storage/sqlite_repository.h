//
// Created by evgen on 14.07.2026.
//

#ifndef ACOUSTID_SERVER_SQ_LITE_REPOSITORY_H
#define ACOUSTID_SERVER_SQ_LITE_REPOSITORY_H
#include <memory>
#include <mutex>
#include <string>

#include <sqlite3.h>

#include "domain/i_track_repository.h"

namespace aid::storage {

    /// SQLite-реализация ITrackRepository.
    ///
    /// - WAL mode для конкурентного доступа между процессами.
    /// - std::mutex для потокобезопасности внутри процесса.
    /// - Таблицы и индексы создаются автоматически при открытии.
    class SQLiteRepository final : public domain::ITrackRepository {
    public:
        /// @param db_path Путь к файлу БД или ":memory:" для in-memory.
        explicit SQLiteRepository(const std::string& db_path);
        ~SQLiteRepository() override;

        // Non-copyable, non-movable (владеет sqlite3* и mutex).
        SQLiteRepository(const SQLiteRepository&) = delete;
        SQLiteRepository& operator=(const SQLiteRepository&) = delete;

        std::size_t AddTrackWithFingerprints(const domain::TrackMetadata& metadata,
                                              const std::vector<core::Fingerprint>& fingerprints) override;

        std::vector<domain::HashLookupResult> FindMatches(const std::vector<uint32_t>& hashes) override;

        std::vector<domain::TrackInfo> GetAllTracks() override;

        void DeleteTrack(std::size_t track_id) override;

    private:
        sqlite3* db_ = nullptr;
        std::mutex mutex_;

        void CreateSchema();
        void EnableWalMode();

        /// Хелпер: выполнить SQL без результата, бросить при ошибке.
        void Execute(const char* sql);
    };

}  // namespace aid::storage

#endif // ACOUSTID_SERVER_SQ_LITE_REPOSITORY_H
