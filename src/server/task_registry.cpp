#include "task_registry.h"

namespace aid::server {

TaskRegistry::TaskRegistry(std::chrono::seconds ttl) : ttl_(ttl) {}

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

void TaskRegistry::Register(const std::string& task_id) {
    std::lock_guard lock(mutex_);
    EvictExpired();
    tasks_[task_id] = TaskState{};
}

void TaskRegistry::SetProcessing(const std::string& task_id) {
    std::lock_guard lock(mutex_);
    auto it = tasks_.find(task_id);
    if (it != tasks_.end()) {
        it->second.status = TaskStatus::PROCESSING;
    }
}

void TaskRegistry::SetDone(const std::string& task_id,
                           std::optional<core::MatchResult> match_result,
                           domain::MatchDiagnostics diagnostics) {
    std::lock_guard lock(mutex_);
    auto it = tasks_.find(task_id);
    if (it != tasks_.end()) {
        it->second.status = TaskStatus::DONE;
        it->second.match_result = std::move(match_result);
        it->second.diagnostics = diagnostics;
    }
}

void TaskRegistry::SetError(const std::string& task_id, const std::string& error_message) {
    std::lock_guard lock(mutex_);
    auto it = tasks_.find(task_id);
    if (it != tasks_.end()) {
        it->second.status = TaskStatus::ERROR;
        it->second.error_message = error_message;
    }
}

std::optional<TaskState> TaskRegistry::Get(const std::string& task_id) const {
    std::lock_guard lock(mutex_);
    auto it = tasks_.find(task_id);
    if (it == tasks_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::size_t TaskRegistry::Size() const {
    std::lock_guard lock(mutex_);
    return tasks_.size();
}

}  // namespace aid::server
