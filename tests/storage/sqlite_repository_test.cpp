//
// Created by evgen on 14.07.2026.
//
//
// Интеграционные тесты для aid::storage::SQLiteRepository.
//
// Используют in-memory SQLite (":memory:") — никаких файлов на диске.

#include "storage/sqlite_repository.h"

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

namespace aid::storage { namespace {

    using core::Fingerprint;
    using domain::HashLookupResult;
    using domain::TrackInfo;
    using domain::TrackMetadata;

    // Хелпер: создаёт репозиторий в памяти.
    std::unique_ptr<SQLiteRepository> MakeRepo() {
        return std::make_unique<SQLiteRepository>(":memory:");
    }

    // Хелпер: простые тестовые fingerprints.
    std::vector<Fingerprint> MakeFingerprints(const std::vector<std::pair<uint32_t, std::size_t>>& data) {
        std::vector<Fingerprint> fps;
        fps.reserve(data.size());
        for (const auto& [hash, frame] : data) {
            fps.push_back(Fingerprint{hash, frame});
        }
        return fps;
    }

    // --- Создание БД ----------------------------------------------------------

    TEST(SQLiteRepositoryTest, OpensInMemoryWithoutError) {
        EXPECT_NO_THROW(MakeRepo());
    }

    // --- AddTrackWithFingerprints ---------------------------------------------

    TEST(SQLiteRepositoryTest, AddTrackReturnsIncrementingIds) {
        auto repo = MakeRepo();

        const auto id1 = repo->AddTrackWithFingerprints({"Song A", "Artist 1", 180.0F}, MakeFingerprints({{100, 0}}));
        const auto id2 = repo->AddTrackWithFingerprints({"Song B", "Artist 2", 240.0F}, MakeFingerprints({{200, 0}}));

        EXPECT_EQ(id1, 1U);
        EXPECT_EQ(id2, 2U);
    }

    TEST(SQLiteRepositoryTest, AddTrackStoresMetadata) {
        auto repo = MakeRepo();

        repo->AddTrackWithFingerprints({"Bohemian Rhapsody", "Queen", 354.5F}, MakeFingerprints({{100, 0}}));

        const auto tracks = repo->GetAllTracks();
        ASSERT_EQ(tracks.size(), 1U);
        EXPECT_EQ(tracks[0].title_, "Bohemian Rhapsody");
        EXPECT_EQ(tracks[0].artist_, "Queen");
        EXPECT_NEAR(tracks[0].duration_sec_, 354.5F, 0.1F);
        EXPECT_GT(tracks[0].indexed_at_, 0);
    }

    TEST(SQLiteRepositoryTest, AddTrackWithEmptyFingerprintsStillCreatesTrack) {
        auto repo = MakeRepo();

        const auto id = repo->AddTrackWithFingerprints({"Empty", "Nobody", 0.0F}, {});
        EXPECT_EQ(id, 1U);

        const auto tracks = repo->GetAllTracks();
        EXPECT_EQ(tracks.size(), 1U);
    }

    // --- FindMatches ----------------------------------------------------------

    TEST(SQLiteRepositoryTest, FindMatchesReturnsCorrectResults) {
        auto repo = MakeRepo();

        const auto fps = MakeFingerprints({{1000, 10}, {2000, 20}, {3000, 30}});
        const auto track_id = repo->AddTrackWithFingerprints({"Test", "Artist", 100.0F}, fps);

        const auto results = repo->FindMatches({2000});
        ASSERT_EQ(results.size(), 1U);
        EXPECT_EQ(results[0].hash_, 2000U);
        EXPECT_EQ(results[0].track_id_, track_id);
        EXPECT_EQ(results[0].track_anchor_frame_, 20U);
    }

    TEST(SQLiteRepositoryTest, FindMatchesReturnsEmptyForUnknownHash) {
        auto repo = MakeRepo();

        repo->AddTrackWithFingerprints({"Test", "Artist", 100.0F}, MakeFingerprints({{1000, 10}}));

        const auto results = repo->FindMatches({9999});
        EXPECT_TRUE(results.empty());
    }

    TEST(SQLiteRepositoryTest, FindMatchesMultipleHashes) {
        auto repo = MakeRepo();

        const auto fps = MakeFingerprints({{100, 5}, {200, 10}, {300, 15}});
        repo->AddTrackWithFingerprints({"Test", "Artist", 100.0F}, fps);

        const auto results = repo->FindMatches({100, 300});
        ASSERT_EQ(results.size(), 2U);
    }

    TEST(SQLiteRepositoryTest, FindMatchesSameHashInMultipleTracks) {
        auto repo = MakeRepo();

        // Два трека с одинаковым хэшем 5000 (но разные фреймы).
        repo->AddTrackWithFingerprints({"Song A", "Artist", 100.0F}, MakeFingerprints({{5000, 10}}));
        repo->AddTrackWithFingerprints({"Song B", "Artist", 200.0F}, MakeFingerprints({{5000, 50}}));

        const auto results = repo->FindMatches({5000});
        ASSERT_EQ(results.size(), 2U);

        // Оба совпадения — разные треки.
        EXPECT_NE(results[0].track_id_, results[1].track_id_);
    }

    TEST(SQLiteRepositoryTest, FindMatchesDuplicateHashInSameTrack) {
        auto repo = MakeRepo();

        // Один трек, один хэш появляется дважды (в разных позициях).
        const auto fps = MakeFingerprints({{7777, 10}, {7777, 40}});
        repo->AddTrackWithFingerprints({"Test", "Artist", 100.0F}, fps);

        const auto results = repo->FindMatches({7777});
        ASSERT_EQ(results.size(), 2U);
        EXPECT_EQ(results[0].track_anchor_frame_ + results[1].track_anchor_frame_, 50U); // 10 + 40
    }

    // --- GetAllTracks ---------------------------------------------------------

    TEST(SQLiteRepositoryTest, GetAllTracksEmptyDB) {
        auto repo = MakeRepo();
        const auto tracks = repo->GetAllTracks();
        EXPECT_TRUE(tracks.empty());
    }

    TEST(SQLiteRepositoryTest, GetAllTracksReturnsAllInOrder) {
        auto repo = MakeRepo();

        repo->AddTrackWithFingerprints({"C Song", "Artist", 100.0F}, MakeFingerprints({{1, 0}}));
        repo->AddTrackWithFingerprints({"A Song", "Artist", 200.0F}, MakeFingerprints({{2, 0}}));
        repo->AddTrackWithFingerprints({"B Song", "Artist", 300.0F}, MakeFingerprints({{3, 0}}));

        const auto tracks = repo->GetAllTracks();
        ASSERT_EQ(tracks.size(), 3U);

        // Порядок по id (порядок вставки).
        EXPECT_EQ(tracks[0].title_, "C Song");
        EXPECT_EQ(tracks[1].title_, "A Song");
        EXPECT_EQ(tracks[2].title_, "B Song");
    }

    // --- DeleteTrack ----------------------------------------------------------

    TEST(SQLiteRepositoryTest, DeleteTrackRemovesTrack) {
        auto repo = MakeRepo();

        const auto id = repo->AddTrackWithFingerprints({"Test", "Artist", 100.0F}, MakeFingerprints({{100, 5}}));
        repo->DeleteTrack(id);

        const auto tracks = repo->GetAllTracks();
        EXPECT_TRUE(tracks.empty());
    }

    TEST(SQLiteRepositoryTest, DeleteTrackCascadesFingerprints) {
        auto repo = MakeRepo();

        const auto id =
            repo->AddTrackWithFingerprints({"Test", "Artist", 100.0F}, MakeFingerprints({{100, 5}, {200, 10}}));

        // Fingerprints существуют до удаления.
        EXPECT_EQ(repo->FindMatches({100}).size(), 1U);

        repo->DeleteTrack(id);

        // Fingerprints удалены каскадно.
        EXPECT_TRUE(repo->FindMatches({100}).empty());
        EXPECT_TRUE(repo->FindMatches({200}).empty());
    }

    TEST(SQLiteRepositoryTest, DeleteNonexistentTrackIsIdempotent) {
        auto repo = MakeRepo();
        EXPECT_NO_THROW(repo->DeleteTrack(999));
    }

    TEST(SQLiteRepositoryTest, DeleteOneTrackDoesNotAffectOthers) {
        auto repo = MakeRepo();

        const auto id1 = repo->AddTrackWithFingerprints({"Keep", "Artist", 100.0F}, MakeFingerprints({{111, 5}}));
        const auto id2 = repo->AddTrackWithFingerprints({"Delete", "Artist", 200.0F}, MakeFingerprints({{222, 10}}));

        repo->DeleteTrack(id2);

        const auto tracks = repo->GetAllTracks();
        ASSERT_EQ(tracks.size(), 1U);
        EXPECT_EQ(tracks[0].id_, id1);

        EXPECT_EQ(repo->FindMatches({111}).size(), 1U);
        EXPECT_TRUE(repo->FindMatches({222}).empty());
    }

    // --- Пустой вход для FindMatches ------------------------------------------

    TEST(SQLiteRepositoryTest, FindMatchesEmptyInputReturnsEmpty) {
        auto repo = MakeRepo();
        repo->AddTrackWithFingerprints({"Test", "Artist", 100.0F}, MakeFingerprints({{100, 5}}));

        const auto results = repo->FindMatches({});
        EXPECT_TRUE(results.empty());
    }

}} // namespace aid::storage