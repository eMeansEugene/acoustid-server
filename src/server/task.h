//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_TASK_H
#define ACOUSTID_SERVER_TASK_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
    struct TaskState {
        TaskStatus status = TaskStatus::PENDING;
        std::optional<domain::MatchOutput> output;  ///< Доступно при DONE.
        std::string error_message;                  ///< Доступно при ERROR.
    };

}  // namespace aid::server
#endif // ACOUSTID_SERVER_TASK_H
