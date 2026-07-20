#include "task_registry.h"

namespace aid::server {

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

}  // namespace aid::server