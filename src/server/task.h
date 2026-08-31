//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_SERVER_TASK_H
#define ACOUSTID_SERVER_SERVER_TASK_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <chrono>

#include "domain/matching_service.h"

namespace aid::server {

    /// Статус задачи распознавания.
    enum class TaskStatus {
        PENDING,     ///< Зарегистрирована, ждёт свободного рабочего потока.
        PROCESSING,  ///< Рабочий поток взял задачу, идёт обработка.
        DONE,        ///< Завершена (совпадение найдено или нет).
        ERROR,       ///< Ошибка при обработке.
    };

    /// Задача, поступающая в очередь.
    struct Task {
        std::string id;                       ///< Идентификатор задачи (см. HttpServer::GenerateTaskId).
        std::vector<uint8_t> audio_bytes;     ///< Содержимое загруженного аудиофрагмента.
    };

    /// Состояние задачи в реестре.
    struct TaskState {
        TaskStatus status = TaskStatus::PENDING;    ///< Текущий статус.
        std::optional<domain::MatchOutput> output;  ///< Доступно при DONE.
        std::string error_message;                  ///< Доступно при ERROR.
        std::chrono::steady_clock::time_point created_at = std::chrono::steady_clock::now(); ///< время создания задачи
    };

}  // namespace aid::server
#endif // ACOUSTID_SERVER_SERVER_TASK_H
