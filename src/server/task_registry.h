#ifndef ACOUSTID_SERVER_SERVER_TASK_REGISTRY_H
#define ACOUSTID_SERVER_SERVER_TASK_REGISTRY_H

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "server/task.h"

namespace aid::server {

/// Потокобезопасное хранилище состояний задач с автоматическим
/// удалением устаревших записей (TTL).
class TaskRegistry {
public:
    /// @param ttl Время жизни завершённой задачи. По умолчанию 1 час.
    explicit TaskRegistry(std::chrono::seconds ttl = std::chrono::seconds(3600));

    /// Зарегистрировать новую задачу со статусом kPending.
    /// Попутно удаляет устаревшие задачи.
    void Register(const std::string& task_id);

    /// Обновить статус на kProcessing.
    void SetProcessing(const std::string& task_id);

    /// Обновить статус на kDone с результатом и диагностикой.
    void SetDone(const std::string& task_id,
                 std::optional<core::MatchResult> match_result,
                 domain::MatchDiagnostics diagnostics);

    /// Обновить статус на kError с сообщением.
    void SetError(const std::string& task_id, const std::string& error_message);

    /// Получить текущее состояние задачи.
    /// Возвращает nullopt, если задача не найдена или устарела.
    std::optional<TaskState> Get(const std::string& task_id) const;

    /// Текущее количество задач в реестре.
    std::size_t Size() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TaskState> tasks_;
    std::chrono::seconds ttl_;

    /// Удаляет завершённые задачи старше TTL. Вызывается под мьютексом.
    void EvictExpired();
};

}  // namespace aid::server

#endif
