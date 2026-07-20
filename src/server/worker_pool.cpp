//
// Created by evgen on 20.07.2026.
//

#include "worker_pool.h"

#include <iostream>

namespace aid::server {

    WorkerPool::WorkerPool(const std::size_t num_workers,
                             TaskQueue& queue,
                             TaskRegistry& registry,
                             domain::MatchingService& matcher)
        : queue_(queue), registry_(registry), matcher_(matcher) {
        workers_.reserve(num_workers);
        for (std::size_t i = 0; i < num_workers; ++i) {
            workers_.emplace_back(&WorkerPool::WorkerLoop, this);
        }
    }

    WorkerPool::~WorkerPool() {
        queue_.Stop();
        for (auto& t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    void WorkerPool::WorkerLoop() const {
        while (true) {
            auto task = queue_.Pop();
            if (!task.has_value()) {
                break;  // очередь остановлена
            }

            const std::string& task_id = task->id;
            registry_.SetProcessing(task_id);

            try {
                domain::MatchOutput output = matcher_.Match(task->audio_bytes);
                registry_.SetDone(task_id, std::move(output));
            } catch (const std::exception& e) {
                registry_.SetError(task_id, e.what());
                std::cerr << "[worker] Task " << task_id << " failed: " << e.what() << "\n";
            } catch (...) {
                registry_.SetError(task_id, "Unknown error");
                std::cerr << "[worker] Task " << task_id << " failed: unknown error\n";
            }
        }
    }

}  // namespace aid::server
