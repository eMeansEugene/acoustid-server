//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_TASK_REGISTRY_H
#define ACOUSTID_SERVER_TASK_REGISTRY_H


#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "server/task.h"

namespace aid::server {

    /// Потокобезопасное хранилище состояний задач.
    /// MatchHandler регистрирует задачу (pending), воркер обновляет статус.
    class TaskRegistry {
    public:
        /// Зарегистрировать новую задачу со статусом kPending.
        void Register(const std::string& task_id);

        /// Обновить статус на kProcessing.
        void SetProcessing(const std::string& task_id);

        /// Обновить статус на kDone с результатом.
        void SetDone(const std::string& task_id, domain::MatchOutput output);

        /// Обновить статус на kError с сообщением.
        void SetError(const std::string& task_id, const std::string& error_message);

        /// Получить текущее состояние задачи.
        /// Возвращает nullopt, если задача не найдена.
        std::optional<TaskState> Get(const std::string& task_id) const;

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::string, TaskState> tasks_;
    };

}  // namespace aid::server

#endif // ACOUSTID_SERVER_TASK_REGISTRY_H
