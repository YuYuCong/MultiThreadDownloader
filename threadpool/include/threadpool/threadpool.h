#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <curl/curl.h>
#include <pthread.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace windmill {

/**
 * Define a thread pool.
 */
class ThreadPool {
 public:
  ThreadPool(unsigned int workers_num = std::thread::hardware_concurrency());
  ~ThreadPool();

  /**
   * submit a task
   */
  template <class F, class... Args>
  auto CommitTask(F &&f, Args &&...args)
      -> std::future<typename std::result_of<F(Args...)>::type>;

  /**
   * Shutdown the thread-pool by user
   */
  void Shutdown();

  /**
   * check if idle
   * @return true if idle, false if busy
   */
  bool CheckIdle();

  /**
   * Monitor of the thread-pool
   */
  void PrintTop();

 private:
  void ThreadProcess();

  struct Top {
    Top(int current_workers_num, int current_running_tasks_num,
        int current_remaining_tasks_num, int total_processed_tasks_num) {
      this->current_workers_num = current_workers_num;
      this->current_running_tasks_num = current_running_tasks_num;
      this->current_remaining_tasks_num = current_remaining_tasks_num;
      this->total_processed_tasks_num = total_processed_tasks_num;
    }
    int current_workers_num;
    int current_running_tasks_num;
    int current_remaining_tasks_num;
    int total_processed_tasks_num;
  };

  std::shared_ptr<Top> GetTop();

  unsigned int workers_num_;

  std::vector<std::thread> workers_;
  std::deque<std::function<void()>> tasks_;

  std::mutex tasks_queue_mutex_;
  std::condition_variable cv_task_;

  bool shutdown_;
  std::atomic_uint current_running_tasks_num_;
  std::atomic_uint total_processed_tasks_num_;
};

template <class F, class... Args>
auto ThreadPool::CommitTask(F &&f, Args &&...args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
  using return_type = typename std::result_of<F(Args...)>::type;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(tasks_queue_mutex_);
    // don't allow enqueueing after stopping the pool
    if (shutdown_) {
      throw std::runtime_error("enqueue on stopped ThreadPool");
    }

    tasks_.emplace_back([task]() { (*task)(); });
  }
  cv_task_.notify_one();
  return res;
}

}  // namespace windmill

#endif
