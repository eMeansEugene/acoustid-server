#include "task_queue.h"

namespace aid::server {

    void TaskQueue::Push(Task task) {
        {
            std::lock_guard lock(mutex_);
            queue_.push(std::move(task));
        }
        cv_.notify_one();
    }

    std::optional<Task> TaskQueue::Pop() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || stopped_; });

        if (queue_.empty()) {
            return std::nullopt;  // stopped и очередь пуста
        }

        Task task = std::move(queue_.front());
        queue_.pop();
        return task;
    }

    void TaskQueue::Stop() {
        {
            std::lock_guard lock(mutex_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

    std::size_t TaskQueue::Size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

}  // namespace aid::server