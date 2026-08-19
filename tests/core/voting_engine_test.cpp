//
// Юнит-тесты для aid::core::VotingEngine (winner/runner-up ratio).
//

#include "core/voting_engine.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

namespace aid::core { namespace {

    TEST(VotingEngineTest, EmptyMatchesReturnsNullopt) {
        const VotingEngine engine;
        EXPECT_FALSE(engine.Vote({}).has_value());
    }

    TEST(VotingEngineTest, ClearWinnerIdentifiedCorrectly) {
        VotingEngineConfig config;
        config.min_votes_ = 5;
        config.min_score_ratio_ = 2.0;
        const VotingEngine engine(config);

        std::vector<HashMatch> matches;
        for (std::size_t i = 0; i < 20; ++i) {
            matches.push_back({7, 100 + i * 5, i * 5});
        }

        const auto result = engine.Vote(matches);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->track_id_, 7U);
        EXPECT_EQ(result->offset_frames_, 100);
        EXPECT_EQ(result->votes_, 20U);
        EXPECT_EQ(result->runner_up_, 0U);
        EXPECT_TRUE(std::isinf(result->score_)); // нет второго кандидата
    }

    TEST(VotingEngineTest, WinnerBeatsNoiseByRatio) {
        VotingEngineConfig config;
        config.min_votes_ = 5;
        config.min_score_ratio_ = 3.0;
        const VotingEngine engine(config);

        std::vector<HashMatch> matches;

        // Победитель: 30 голосов.
        for (std::size_t i = 0; i < 30; ++i) {
            matches.push_back({3, 200 + i * 3, i * 3});
        }

        // Второе место: 5 голосов (одна пара track_id + Δ).
        for (std::size_t i = 0; i < 5; ++i) {
            matches.push_back({8, 500 + i, i});
        }

        const auto result = engine.Vote(matches);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->track_id_, 3U);
        EXPECT_EQ(result->votes_, 30U);
        EXPECT_EQ(result->runner_up_, 5U);
        EXPECT_DOUBLE_EQ(result->score_, 6.0);
    }

    TEST(VotingEngineTest, BelowMinVotesReturnsNullopt) {
        VotingEngineConfig config;
        config.min_votes_ = 50;
        config.min_score_ratio_ = 1.0;
        const VotingEngine engine(config);

        std::vector<HashMatch> matches;
        for (std::size_t i = 0; i < 10; ++i) {
            matches.push_back({1, 50 + i, i});
        }

        EXPECT_FALSE(engine.Vote(matches).has_value());
    }

    TEST(VotingEngineTest, BelowMinRatioReturnsNullopt) {
        VotingEngineConfig config;
        config.min_votes_ = 1;
        config.min_score_ratio_ = 5.0;
        const VotingEngine engine(config);

        std::vector<HashMatch> matches;

        // Первое место: 10 голосов.
        for (std::size_t i = 0; i < 10; ++i) {
            matches.push_back({1, 100 + i, i});
        }

        // Второе место: 8 голосов. Ratio = 1.25 < 5.0.
        for (std::size_t i = 0; i < 8; ++i) {
            matches.push_back({2, 300 + i, i});
        }

        EXPECT_FALSE(engine.Vote(matches).has_value());
    }

    TEST(VotingEngineTest, TwoTracksStrongerWins) {
        VotingEngineConfig config;
        config.min_votes_ = 1;
        config.min_score_ratio_ = 1.5;
        const VotingEngine engine(config);

        std::vector<HashMatch> matches;

        for (std::size_t i = 0; i < 5; ++i) {
            matches.push_back({1, 50 + i, i});
        }
        for (std::size_t i = 0; i < 12; ++i) {
            matches.push_back({2, 300 + i * 2, i * 2});
        }

        const auto result = engine.Vote(matches);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->track_id_, 2U);
        EXPECT_EQ(result->votes_, 12U);
        EXPECT_EQ(result->runner_up_, 5U);
        EXPECT_NEAR(result->score_, 2.4, 0.01);
    }

    TEST(VotingEngineTest, SameTrackDifferentDeltaSeparate) {
        VotingEngineConfig config;
        config.min_votes_ = 1;
        config.min_score_ratio_ = 1.0;
        const VotingEngine engine(config);

        std::vector<HashMatch> matches;

        // Δ = 100: 8 голосов.
        for (std::size_t i = 0; i < 8; ++i) {
            matches.push_back({1, 100 + i, i});
        }
        // Δ = 500: 3 голоса — runner_up.
        for (std::size_t i = 0; i < 3; ++i) {
            matches.push_back({1, 500 + i, i});
        }

        const auto result = engine.Vote(matches);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->votes_, 8U);
        EXPECT_EQ(result->runner_up_, 3U);
    }

    TEST(VotingEngineTest, SingleMatchAboveThreshold) {
        VotingEngineConfig config;
        config.min_votes_ = 1;
        config.min_score_ratio_ = 1.0;
        const VotingEngine engine(config);

        std::vector<HashMatch> matches = {{42, 150, 10}};

        const auto result = engine.Vote(matches);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->track_id_, 42U);
        EXPECT_EQ(result->votes_, 1U);
        EXPECT_EQ(result->runner_up_, 0U);
        EXPECT_TRUE(std::isinf(result->score_));
    }

}} // namespace aid::core