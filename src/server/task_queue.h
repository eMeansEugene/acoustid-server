//
// Created by evgen on 20.07.2026.
//

#ifndef ACOUSTID_SERVER_TASK_QUEUE_H
#define ACOUSTID_SERVER_TASK_QUEUE_H

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

#include "server/task.h"

namespace aid::server {

    /// Thread-safe очередь задач с блокирующим Pop и корректным завершением.
    class TaskQueue {
    public:
        /// Добавить задачу. Безопасно вызывать из любого потока.
        void Push(Task task);

        /// Забрать задачу. Блокируется, если очередь пуста.
        /// Возвращает nullopt, если вызван Stop() и очередь пуста.
        std::optional<Task> Pop();

        /// Сигнал завершения: все ждущие Pop() разблокируются и получат nullopt.
        void Stop();

        std::size_t Size() const;

    private:
        mutable std::mutex mutex_;
        std::condition_variable cv_;
        std::queue<Task> queue_;
        bool stopped_ = false;
    };

}  // namespace aid::server

#endif // ACOUSTID_SERVER_TASK_QUEUE_H
