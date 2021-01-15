#include <pthread.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace windmill {
namespace common {
/**
 * Define a thread pool.
 */
class ThreadPool {
 public:
  ThreadPool(unsigned int n = std::thread::hardware_concurrency());
  ~ThreadPool();

  template <class F, class... Args>
  auto CommitTask(F &&f, Args &&... args)
      -> std::future<typename std::result_of<F(Args...)>::type>;

  bool Finished();
  unsigned int GetProcessed();
 private:
  std::vector<std::thread> workers_;
  std::deque<std::function<void()>> tasks_;
  std::mutex queue_mutex_;
  std::condition_variable cv_task_;
  unsigned int busy_;
  std::atomic_uint processed_;
  bool shutdown_;

  void ThreadProcess();
};

template <class F, class... Args>
auto ThreadPool::CommitTask(F &&f, Args &&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {
  using return_type = typename std::result_of<F(Args...)>::type;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    // don't allow enqueueing after stopping the pool
    if (shutdown_) throw std::runtime_error("enqueue on stopped ThreadPool");

    tasks_.emplace_back([task]() { (*task)(); });
  }
  cv_task_.notify_one();
  return res;
}

ThreadPool::ThreadPool(unsigned int n) : busy_(), processed_(), shutdown_() {
  for (unsigned int i = 0; i < n; ++i) {
    workers_.emplace_back(std::bind(&ThreadPool::ThreadProcess, this));
  }
}

ThreadPool::~ThreadPool() {
  std::unique_lock<std::mutex> latch(queue_mutex_);
  shutdown_ = true;
  latch.unlock();
  cv_task_.notify_all();
  for (auto &worker : workers_) worker.join();
}

void ThreadPool::ThreadProcess() {
  while (true) {
    std::unique_lock<std::mutex> latch(queue_mutex_);
    cv_task_.wait(latch, [this]() { return shutdown_ || !tasks_.empty(); });
    if (shutdown_) {
      break;
    }

    if (!tasks_.empty()) {
      ++busy_;
      auto task = tasks_.front();
      tasks_.pop_front();
      latch.unlock();
      task();
      ++processed_;
      latch.lock();
      --busy_;
    }
  }
}

bool ThreadPool::Finished() {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  return tasks_.empty() && (busy_ == 0);
}

unsigned int ThreadPool::GetProcessed() { return processed_; }

}  // namespace common
}  // namespace windmill
