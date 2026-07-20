//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_WORKER_POOL_H
#define ACOUSTID_SERVER_WORKER_POOL_H

#include <cstddef>
#include <thread>
#include <vector>

#include "domain/matching_service.h"
#include "server/task_queue.h"
#include "server/task_registry.h"

namespace aid::server {

    /// Пул воркеров, обрабатывающих задачи матчинга.
    ///
    /// Каждый воркер в цикле: Pop() из очереди → SetProcessing() →
    /// MatchingService::Match() → SetDone() / SetError().
    /// Завершение: TaskQueue::Stop() → все воркеры получают nullopt → join.
    class WorkerPool {
    public:
        /// @param num_workers Количество потоков.
        /// @param queue       Очередь задач (не владеет).
        /// @param registry    Реестр состояний (не владеет).
        /// @param matcher     Сервис матчинга (не владеет).
        WorkerPool(std::size_t num_workers,
                   TaskQueue& queue,
                   TaskRegistry& registry,
                   domain::MatchingService& matcher);

        /// Останавливает очередь и дожидается завершения всех потоков.
        ~WorkerPool();

        // Non-copyable, non-movable.
        WorkerPool(const WorkerPool&) = delete;
        WorkerPool& operator=(const WorkerPool&) = delete;

    private:
        TaskQueue& queue_;
        TaskRegistry& registry_;
        domain::MatchingService& matcher_;
        std::vector<std::thread> workers_;

        void WorkerLoop() const;
    };

}  // namespace aid::server
#endif // ACOUSTID_SERVER_WORKER_POOL_H
