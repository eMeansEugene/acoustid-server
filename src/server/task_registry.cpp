#include "task_registry.h"

namespace aid::server {

    TaskRegistry::TaskRegistry(const std::chrono::seconds ttl) : ttl_(ttl) {}

    void TaskRegistry::Register(const std::string& task_id) {
        std::lock_guard lock(mutex_);
        tasks_[task_id] = TaskState{TaskStatus::PENDING, std::nullopt, ""};
    }

    void TaskRegistry::SetProcessing(const std::string& task_id) {
        std::lock_guard lock(mutex_);
        if (const auto it = tasks_.find(task_id); it != tasks_.end()) {
            it->second.status = TaskStatus::PROCESSING;
        }
    }

    void TaskRegistry::SetDone(const std::string& task_id, domain::MatchOutput output) {
        std::lock_guard lock(mutex_);
        if (const auto it = tasks_.find(task_id); it != tasks_.end()) {
            it->second.status = TaskStatus::DONE;
            it->second.output = std::move(output);
        }
    }

    void TaskRegistry::SetError(const std::string& task_id, const std::string& error_message) {
        std::lock_guard lock(mutex_);
        if (const auto it = tasks_.find(task_id); it != tasks_.end()) {
            it->second.status = TaskStatus::ERROR;
            it->second.error_message = error_message;
        }
    }

    std::optional<TaskState> TaskRegistry::Get(const std::string& task_id) const {
        std::lock_guard lock(mutex_);
        const auto it = tasks_.find(task_id);
        if (it == tasks_.end()) {
            return std::nullopt;
        }
        return it->second;
    }
    void TaskRegistry::EvictExpired() {
        // Вызывается под мьютексом. Удаляет только завершённые задачи (done/error)
        // старше TTL. Pending и processing не трогаем — они ещё в работе.
        const auto now = std::chrono::steady_clock::now();
        for (auto it = tasks_.begin(); it != tasks_.end();) {
            const auto& state = it->second;
            const bool finished = (state.status == TaskStatus::DONE || state.status == TaskStatus::ERROR);
            const bool expired = (now - state.created_at) > ttl_;
            if (finished && expired) {
                it = tasks_.erase(it);
            } else {
                ++it;
            }
        }
    }
}
