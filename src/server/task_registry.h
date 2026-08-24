//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_SERVER_TASK_REGISTRY_H
#define ACOUSTID_SERVER_SERVER_TASK_REGISTRY_H

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "server/task.h"

namespace aid::server {

    /// Потокобезопасное хранилище состояний задач.
    /// HttpServer::HandleMatch регистрирует задачу (pending), рабочий поток
    /// из WorkerPool обновляет статус по мере обработки.
    class TaskRegistry {
    public:
        /// Зарегистрировать новую задачу со статусом TaskStatus::PENDING.
        /// @param task_id Идентификатор задачи.
        void Register(const std::string& task_id);

        /// Обновить статус на TaskStatus::PROCESSING.
        /// @param task_id Идентификатор задачи.
        void SetProcessing(const std::string& task_id);

        /// Обновить статус на TaskStatus::DONE с результатом.
        /// @param task_id Идентификатор задачи.
        /// @param output Результат распознавания.
        void SetDone(const std::string& task_id, domain::MatchOutput output);

        /// Обновить статус на TaskStatus::ERROR с сообщением.
        /// @param task_id Идентификатор задачи.
        /// @param error_message Текст ошибки.
        void SetError(const std::string& task_id, const std::string& error_message);

        /// Получить текущее состояние задачи.
        /// @param task_id Идентификатор задачи.
        /// @return Состояние задачи или nullopt, если задача не найдена.
        std::optional<TaskState> Get(const std::string& task_id) const;

    private:
        mutable std::mutex mutex_;
        std::unordered_map<std::string, TaskState> tasks_;
    };

}  // namespace aid::server

#endif // ACOUSTID_SERVER_SERVER_TASK_REGISTRY_H
