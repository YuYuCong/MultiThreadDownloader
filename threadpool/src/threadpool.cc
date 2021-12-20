#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

#include "threadpool/threadpool.h"

namespace {
const unsigned int kMaxAllowedWorkers = 64;  // max allowed threads
}  // namespace

namespace windmill {
ThreadPool::ThreadPool(unsigned int workers_num)
    : workers_num_(workers_num < kMaxAllowedWorkers ? workers_num
                                                    : kMaxAllowedWorkers),
      shutdown_(false),
      current_running_tasks_num_(0),
      total_processed_tasks_num_(0) {
  for (unsigned int i = 0; i < workers_num_; ++i) {
    workers_.emplace_back(std::bind(&ThreadPool::ThreadProcess, this));
  }
}

ThreadPool::~ThreadPool() {
  std::unique_lock<std::mutex> latch(tasks_queue_mutex_);
  shutdown_ = true;
  latch.unlock();
  cv_task_.notify_all();
  for (auto &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void ThreadPool::ThreadProcess() {
  while (true) {
    std::unique_lock<std::mutex> latch(tasks_queue_mutex_);
    cv_task_.wait(latch, [this]() { return shutdown_ || !tasks_.empty(); });
    if (shutdown_) {
      break;
    }

    if (!tasks_.empty()) {
      ++current_running_tasks_num_;
      auto task = tasks_.front();
      tasks_.pop_front();
      latch.unlock();
      task();
      ++total_processed_tasks_num_;
      latch.lock();
      --current_running_tasks_num_;
    }
  }
}

void ThreadPool::Shutdown() {
  printf("shutdown now\n");
  std::unique_lock<std::mutex> latch(tasks_queue_mutex_);
  shutdown_ = true;
  latch.unlock();
  cv_task_.notify_all();
  for (auto &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

bool ThreadPool::CheckIdle() {
  std::unique_lock<std::mutex> lock(tasks_queue_mutex_);
  return (tasks_.empty() && (current_running_tasks_num_ == 0));
}

std::shared_ptr<ThreadPool::Top> ThreadPool::GetTop() {
  std::unique_lock<std::mutex> latch(tasks_queue_mutex_);
  return std::make_shared<Top>(Top(workers_.size(), current_running_tasks_num_,
                                   tasks_.size(), total_processed_tasks_num_));
}

void ThreadPool::PrintTop() {
  auto thread_pool_monitor = GetTop();
  printf(
      "[ThreadPool Top] current_workers:%d | current_working:%d | "
      "remaining_tasks:%d | "
      "total_processed_tasks_num:%d\n",
      thread_pool_monitor->current_workers_num,
      thread_pool_monitor->current_running_tasks_num,
      thread_pool_monitor->current_remaining_tasks_num,
      thread_pool_monitor->total_processed_tasks_num);
}

}  // namespace windmill
