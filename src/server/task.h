#ifndef ACOUSTID_SERVER_SERVER_TASK_H
#define ACOUSTID_SERVER_SERVER_TASK_H

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/voting_engine.h"
#include "domain/matching_service.h"

namespace aid::server {

/// Статус задачи матчинга.
enum class TaskStatus {
    PENDING,     ///< Зарегистрирована, ждёт воркера.
    PROCESSING,  ///< Воркер взял, идёт обработка.
    DONE,        ///< Завершена (match или no match).
    ERROR,       ///< Ошибка при обработке.
};

/// Задача, поступающая в очередь.
struct Task {
    std::string id;
    std::vector<uint8_t> audio_bytes;
};

/// Состояние задачи в реестре.
/// Хранит только результат и диагностику — спектрограмма и пики
/// не сохраняются (визуализация выполняется на клиенте).
struct TaskState {
    TaskStatus status = TaskStatus::PENDING;
    std::optional<core::MatchResult> match_result;  ///< Результат голосования.
    domain::MatchDiagnostics diagnostics;           ///< Статистика пайплайна.
    std::string error_message;                      ///< Доступно при kError.
    std::chrono::steady_clock::time_point created_at = std::chrono::steady_clock::now();
};

}
#endif
// namespace aid::server
