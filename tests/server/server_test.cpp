//
// Created by evgen on 20.07.2026.
//
//
// Тесты инфраструктуры сервера: TaskQueue, TaskRegistry, WorkerPool.
// HTTP-обработчики тестируются через e2e (отдельно).

#include "server/task.h"
#include "server/task_queue.h"
#include "server/task_registry.h"
#include "server/worker_pool.h"

#include "audio/audio_decoder.h"
#include "core/audio_fingerprint_engine.h"
#include "core/voting_engine.h"
#include "domain/matching_service.h"
#include "storage/sqlite_repository.h"

#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace aid::server {
namespace {

// ---- TaskQueue -----------------------------------------------------------

TEST(TaskQueueTest, PushPopSingleItem) {
    TaskQueue queue;
    queue.Push(Task{"task-1", {1, 2, 3}});

    auto task = queue.Pop();
    ASSERT_TRUE(task.has_value());
    EXPECT_EQ(task->id, "task-1");
    EXPECT_EQ(task->audio_bytes.size(), 3U);
}

TEST(TaskQueueTest, FIFOOrder) {
    TaskQueue queue;
    queue.Push(Task{"a", {}});
    queue.Push(Task{"b", {}});
    queue.Push(Task{"c", {}});

    EXPECT_EQ(queue.Pop()->id, "a");
    EXPECT_EQ(queue.Pop()->id, "b");
    EXPECT_EQ(queue.Pop()->id, "c");
}

TEST(TaskQueueTest, SizeTracksItems) {
    TaskQueue queue;
    EXPECT_EQ(queue.Size(), 0U);

    queue.Push(Task{"1", {}});
    queue.Push(Task{"2", {}});
    EXPECT_EQ(queue.Size(), 2U);

    queue.Pop();
    EXPECT_EQ(queue.Size(), 1U);
}

TEST(TaskQueueTest, StopUnblocksWaitingPop) {
    TaskQueue queue;

    std::optional<Task> result;
    std::thread consumer([&] { result = queue.Pop(); });

    // Дать потоку время заблокироваться.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    queue.Stop();
    consumer.join();

    EXPECT_FALSE(result.has_value());
}

TEST(TaskQueueTest, StopAfterPushStillReturnsItem) {
    TaskQueue queue;
    queue.Push(Task{"x", {}});
    queue.Stop();

    // Элемент уже в очереди — должен вернуться, несмотря на Stop.
    auto task = queue.Pop();
    ASSERT_TRUE(task.has_value());
    EXPECT_EQ(task->id, "x");
}

TEST(TaskQueueTest, ConcurrentPushPop) {
    TaskQueue queue;
    constexpr int kItems = 100;

    std::thread producer([&] {
        for (int i = 0; i < kItems; ++i) {
            queue.Push(Task{std::to_string(i), {}});
        }
    });

    std::vector<std::string> received;
    std::thread consumer([&] {
        for (int i = 0; i < kItems; ++i) {
            auto task = queue.Pop();
            if (task) received.push_back(task->id);
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(received.size(), kItems);
}

// ---- TaskRegistry --------------------------------------------------------

TEST(TaskRegistryTest, RegisterCreatesPendingState) {
    TaskRegistry registry;
    registry.Register("t1");

    auto state = registry.Get("t1");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->status, TaskStatus::PENDING);
}

TEST(TaskRegistryTest, UnknownTaskReturnsNullopt) {
    TaskRegistry registry;
    EXPECT_FALSE(registry.Get("nonexistent").has_value());
}

TEST(TaskRegistryTest, StatusTransitions) {
    TaskRegistry registry;
    registry.Register("t1");

    registry.SetProcessing("t1");
    EXPECT_EQ(registry.Get("t1")->status, TaskStatus::PROCESSING);
    core::FingerprintResult fr{core::Spectrogram(0, 0), {}, {}};
    registry.SetDone("t1", domain::MatchOutput{std::move(fr), std::nullopt});
    EXPECT_EQ(registry.Get("t1")->status, TaskStatus::DONE);
}

TEST(TaskRegistryTest, ErrorStateStoresMessage) {
    TaskRegistry registry;
    registry.Register("t1");

    registry.SetError("t1", "decode failed");
    auto state = registry.Get("t1");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->status, TaskStatus::ERROR);
    EXPECT_EQ(state->error_message, "decode failed");
}

TEST(TaskRegistryTest, DoneStateStoresOutput) {
    TaskRegistry registry;
    registry.Register("t1");

    core::MatchResult mr{42, 100, 0.85, 200};
    core::FingerprintResult fr{core::Spectrogram(0, 0), {}, {}};
    registry.SetDone("t1", domain::MatchOutput{std::move(fr), mr});

    auto state = registry.Get("t1");
    ASSERT_TRUE(state.has_value());
    ASSERT_TRUE(state->output.has_value());
    ASSERT_TRUE(state->output->match_result.has_value());
    EXPECT_EQ(state->output->match_result->track_id_, 42U);
}

}  // namespace
}  // namespace aid::server