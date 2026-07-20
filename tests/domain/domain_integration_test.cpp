//
// Интеграционные тесты для domain-оркестраторов.
//
// Используют синтетический WAV (сумма синусов), реальный DSP-пайплайн
// и in-memory SQLite. Полностью детерминированы.

#include "core/audio_fingerprint_engine.h"
#include "domain/indexing_service.h"
#include "domain/matching_service.h"
#include "storage/sqlite_repository.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

namespace {

// ---- Хелперы для построения синтетического WAV ----------------------------

void WriteU16(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
void WriteU32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}
void WriteTag(std::vector<uint8_t>& buf, const char* tag) {
    for (int i = 0; i < 4; ++i) buf.push_back(static_cast<uint8_t>(tag[i]));
}
void WriteI16(std::vector<uint8_t>& buf, int16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

/// Строит моно 16-bit PCM WAV из float-сэмплов.
std::vector<uint8_t> BuildWav(const std::vector<float>& samples, uint32_t sample_rate) {
    const uint32_t data_size = static_cast<uint32_t>(samples.size()) * 2;
    std::vector<uint8_t> buf;
    buf.reserve(44 + data_size);
    WriteTag(buf, "RIFF"); WriteU32(buf, 36 + data_size); WriteTag(buf, "WAVE");
    WriteTag(buf, "fmt "); WriteU32(buf, 16);
    WriteU16(buf, 1); WriteU16(buf, 1);  // PCM, mono
    WriteU32(buf, sample_rate); WriteU32(buf, sample_rate * 2);
    WriteU16(buf, 2); WriteU16(buf, 16);
    WriteTag(buf, "data"); WriteU32(buf, data_size);
    for (float s : samples) {
        auto v = static_cast<int16_t>(std::max(-1.0F, std::min(1.0F, s)) * 32767.0F);
        WriteI16(buf, v);
    }
    return buf;
}

/// Генерирует сигнал: сумма синусов на заданных частотах.
std::vector<float> MakeSignal(const std::vector<float>& freqs, float duration_sec, uint32_t sample_rate) {
    const auto n = static_cast<std::size_t>(duration_sec * static_cast<float>(sample_rate));
    std::vector<float> samples(n, 0.0F);
    const float amplitude = 0.5F / static_cast<float>(freqs.size());
    for (const float freq : freqs) {
        for (std::size_t i = 0; i < n; ++i) {
            samples[i] += amplitude * std::sin(2.0F * static_cast<float>(M_PI) * freq *
                                                static_cast<float>(i) / static_cast<float>(sample_rate));
        }
    }
    return samples;
}

/// Извлекает подмассив сэмплов (фрагмент из середины трека).
std::vector<float> ExtractFragment(const std::vector<float>& signal, const float start_sec, const float duration_sec, uint32_t sample_rate) {
    const auto start = static_cast<std::size_t>(start_sec * static_cast<float>(sample_rate));
    const auto length = static_cast<std::size_t>(duration_sec * static_cast<float>(sample_rate));
    if (start + length > signal.size()) {
        return {};
    }
    return {signal.begin() + static_cast<std::ptrdiff_t>(start),
            signal.begin() + static_cast<std::ptrdiff_t>(start + length)};
}

// ---- Общие конфиги для тестов --------------------------------------------

constexpr uint32_t SAMPLE_RATE = 44100;

aid::core::PeakExtractorConfig TestPeakConfig() {
    aid::core::PeakExtractorConfig config;
    config.frame_radius_ = 2;
    config.bin_radius_ = 2;
    config.offset_db_ = 6.0F;
    config.zone_frames_ = 43;  // ~1 секунда при hop=1024, sr=44100
    config.peak_limit_ = 50;
    return config;
}

// ---- AudioFingerprintEngine unit tests -----------------------------------

TEST(AudioFingerprintEngineTest, SineSignalProducesFingerprints) {
    aid::core::AudioFingerprintEngine engine({}, TestPeakConfig(), {});

    const auto signal = MakeSignal({440.0F, 880.0F, 1320.0F}, 3.0F, SAMPLE_RATE);
    const auto [spectrogram, peaks, fingerprints] = engine.Process(signal);

    EXPECT_GT(spectrogram.NumFrames(), 0U);
    EXPECT_GT(peaks.size(), 0U);
    EXPECT_GT(fingerprints.size(), 0U);
}

TEST(AudioFingerprintEngineTest, EmptySignalProducesNothing) {
    const aid::core::AudioFingerprintEngine engine({}, TestPeakConfig(), {});

    const auto result = engine.Process({});

    EXPECT_EQ(result.spectrogram.NumFrames(), 0U);
    EXPECT_TRUE(result.peaks.empty());
    EXPECT_TRUE(result.fingerprints.empty());
}

TEST(AudioFingerprintEngineTest, ShortSignalProducesNothing) {
    aid::core::AudioFingerprintEngine engine({}, TestPeakConfig(), {});

    // Короче одного фрейма (2048 сэмплов).
    const std::vector<float> short_signal(1000, 0.5F);
    const auto result = engine.Process(short_signal);

    EXPECT_EQ(result.spectrogram.NumFrames(), 0U);
    EXPECT_TRUE(result.fingerprints.empty());
}

// ---- IndexingService + MatchingService integration -----------------------

class DomainIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        repo_ = std::make_unique<aid::storage::SQLiteRepository>(":memory:");
        engine_ = std::make_unique<aid::core::AudioFingerprintEngine>(
            aid::core::FftEngineConfig{}, TestPeakConfig(), aid::core::HashGeneratorConfig{});

        indexing_ = std::make_unique<aid::domain::IndexingService>(decoder_, *engine_, *repo_);

        aid::core::VotingEngineConfig vote_config;
        vote_config.min_confidence_ = 0.05;
        voter_ = std::make_unique<aid::core::VotingEngine>(vote_config);

        matching_ = std::make_unique<aid::domain::MatchingService>(decoder_, *engine_, *repo_, *voter_);
    }

    aid::audio::AudioDecoder decoder_;
    std::unique_ptr<aid::storage::SQLiteRepository> repo_;
    std::unique_ptr<aid::core::AudioFingerprintEngine> engine_;
    std::unique_ptr<aid::core::VotingEngine> voter_;
    std::unique_ptr<aid::domain::IndexingService> indexing_;
    std::unique_ptr<aid::domain::MatchingService> matching_;
};

TEST_F(DomainIntegrationTest, IndexTrackStoresMetadataAndFingerprints) {
    const auto signal = MakeSignal({440.0F, 880.0F}, 3.0F, SAMPLE_RATE);
    const auto wav = BuildWav(signal, SAMPLE_RATE);

    const auto result = indexing_->IndexFromBytes(wav, {"Test Song", "Test Artist", 0.0F});

    EXPECT_EQ(result.track_id, 1U);
    EXPECT_GT(result.fingerprint_count, 0U);

    const auto tracks = repo_->GetAllTracks();
    ASSERT_EQ(tracks.size(), 1U);
    EXPECT_EQ(tracks[0].title_, "Test Song");
    EXPECT_EQ(tracks[0].artist_, "Test Artist");
}

TEST_F(DomainIntegrationTest, MatchFragmentFindsCorrectTrack) {
    // Сигнал 1: три частоты.
    const auto signal1 = MakeSignal({440.0F, 880.0F, 1320.0F}, 5.0F, SAMPLE_RATE);
    const auto wav1 = BuildWav(signal1, SAMPLE_RATE);
    indexing_->IndexFromBytes(wav1, {"Song A", "Artist", 0.0F});

    // Сигнал 2: другие частоты (чтобы был альтернативный кандидат).
    const auto signal2 = MakeSignal({300.0F, 600.0F, 900.0F}, 5.0F, SAMPLE_RATE);
    const auto wav2 = BuildWav(signal2, SAMPLE_RATE);
    indexing_->IndexFromBytes(wav2, {"Song B", "Artist", 0.0F});

    // Фрагмент из середины сигнала 1.
    const auto fragment = ExtractFragment(signal1, 1.5F, 3.0F, SAMPLE_RATE);
    const auto frag_wav = BuildWav(fragment, SAMPLE_RATE);

    const auto output = matching_->Match(frag_wav);

    ASSERT_TRUE(output.match_result.has_value());
    EXPECT_EQ(output.match_result->track_id_, 1U);  // Song A
    EXPECT_GT(output.match_result->confidence_, 0.05);
    EXPECT_GT(output.match_result->votes_, 0U);

    // Промежуточные данные для визуализации доступны.
    EXPECT_GT(output.fingerprint_result.spectrogram.NumFrames(), 0U);
    EXPECT_GT(output.fingerprint_result.peaks.size(), 0U);
}

TEST_F(DomainIntegrationTest, MatchUnknownFragmentReturnsNullopt) {
    // Проиндексирован трек с одними частотами.
    const auto signal = MakeSignal({440.0F, 880.0F}, 3.0F, SAMPLE_RATE);
    const auto wav = BuildWav(signal, SAMPLE_RATE);
    indexing_->IndexFromBytes(wav, {"Known", "Artist", 0.0F});

    // Фрагмент с совсем другими частотами — не должен совпасть.
    const auto unknown = MakeSignal({2000.0F, 3000.0F, 4000.0F}, 3.0F, SAMPLE_RATE);
    const auto unknown_wav = BuildWav(unknown, SAMPLE_RATE);

    const auto output = matching_->Match(unknown_wav);

    EXPECT_FALSE(output.match_result.has_value());
}

TEST_F(DomainIntegrationTest, MatchEmptyDatabaseReturnsNullopt) {
    // Пустая БД — ничего не проиндексировано.
    const auto signal = MakeSignal({440.0F}, 3.0F, SAMPLE_RATE);
    const auto wav = BuildWav(signal, SAMPLE_RATE);

    const auto output = matching_->Match(wav);

    EXPECT_FALSE(output.match_result.has_value());
}

TEST_F(DomainIntegrationTest, IndexMultipleTracksAndMatchEach) {
    // Три трека с разными частотами.
    struct TrackDef {
        std::string title;
        std::vector<float> freqs;
    };
    const TrackDef defs[] = {
        {"Track A", {440.0F, 880.0F}},
        {"Track B", {300.0F, 700.0F, 1100.0F}},
        {"Track C", {500.0F, 1000.0F, 1500.0F, 2000.0F}},
    };

    for (const auto& def : defs) {
        const auto signal = MakeSignal(def.freqs, 5.0F, SAMPLE_RATE);
        const auto wav = BuildWav(signal, SAMPLE_RATE);
        indexing_->IndexFromBytes(wav, {def.title, "Artist", 0.0F});
    }

    EXPECT_EQ(repo_->GetAllTracks().size(), 3U);

    // Для каждого трека: вырезать фрагмент, матчить, проверить track_id.
    for (std::size_t i = 0; i < 3; ++i) {
        const auto signal = MakeSignal(defs[i].freqs, 5.0F, SAMPLE_RATE);
        const auto fragment = ExtractFragment(signal, 1.0F, 3.0F, SAMPLE_RATE);
        const auto frag_wav = BuildWav(fragment, SAMPLE_RATE);

        const auto output = matching_->Match(frag_wav);

        ASSERT_TRUE(output.match_result.has_value())
            << "Failed to match fragment of " << defs[i].title;
        EXPECT_EQ(output.match_result->track_id_, i + 1)
            << "Wrong track for " << defs[i].title;
    }
}

}  // namespace