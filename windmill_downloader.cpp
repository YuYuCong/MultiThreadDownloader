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




#define LOG(ERROR) std::cout
#define PrintEndl std::cout << std::endl

namespace windmill {
namespace utility {

const int kMaxAllowedReconnectTimes = 15;

/**
 * Define a WebDownloader.
 */
class WebDownloader {
 public:
  WebDownloader();
  ~WebDownloader();
  bool DownloadMission(const int thread_num, const std::string &url,
                       const std::string &out_file);

 private:
  struct DownloadNode {
    FILE *fp;
    long start_pos;
    long end_pos;
    int reconnect_times;
  };

  long GetDownloadFileLenth(const std::string url);

  static size_t MultiThreadWriterCallback(void *ptr, size_t size, size_t nmemb,
                                          void *userdata);

  static int MultiThreadDownloadProgressCallback(void *ptr,
                                                 double total_to_download,
                                                 double now_downloaded,
                                                 double total_to_upload,
                                                 double now_uploaded);

  static void PrintDownloadProgress(double now_downloaded,
                                    double total_to_download);

  static int RangeDownloadThreadIns(WebDownloader *pthis,
                                    const std::string &url,
                                    std::shared_ptr<DownloadNode> pnode);

  int RangeDownloadThread(const std::string &url,
                          std::shared_ptr<DownloadNode> pnode);
  std::atomic<int> downloading_thread_count_;
  std::atomic<int> failed_nodes_count_;
  int max_reconnect_times_;

  static long total_size_to_download_;
  static int total_download_last_percent_;
  static std::unordered_map<uint64_t, double> download_process_statistics_;
  static std::mutex downloadprocess_mutex_;
  static std::mutex writer_mutex_;

  std::mutex node_manager_mutex_;
  std::vector<std::shared_ptr<DownloadNode>> failed_nodes_;
};

WebDownloader::WebDownloader()
    : downloading_thread_count_(0),
      failed_nodes_count_(0),
      max_reconnect_times_(kMaxAllowedReconnectTimes) {}

WebDownloader::~WebDownloader() {}

long WebDownloader::GetDownloadFileLenth(const std::string url) {
  double download_file_lenth = 0;
  CURL *curl = curl_easy_init();
  if (NULL == curl) {
    return CURLE_FAILED_INIT;
  }
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HEADER, 1);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
  if (CURLE_OK == curl_easy_perform(curl)) {
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                      &download_file_lenth);
  } else {
    download_file_lenth = -1;
  }
  LOG(ERROR) << "download_file_lenth: " << download_file_lenth;
  PrintEndl;
  return download_file_lenth;
}

size_t WebDownloader::MultiThreadWriterCallback(void *ptr, size_t size,
                                                size_t nmemb, void *userdata) {
  DownloadNode *node = (DownloadNode *)userdata;
  size_t written = 0;
  writer_mutex_.lock();
  if (node->start_pos + size * nmemb <= node->end_pos) {
    fseek(node->fp, node->start_pos, SEEK_SET);
    written = fwrite(ptr, size, nmemb, node->fp);
    node->start_pos += size * nmemb;
  } else {
    fseek(node->fp, node->start_pos, SEEK_SET);
    written = fwrite(ptr, 1, node->end_pos - node->start_pos + 1, node->fp);
    node->start_pos = node->end_pos;
  }
  writer_mutex_.unlock();
  return written;
}

int WebDownloader::MultiThreadDownloadProgressCallback(void *ptr,
                                                       double total_to_download,
                                                       double now_downloaded,
                                                       double total_to_upload,
                                                       double now_uploaded) {
  auto current_thread_id = pthread_self();

  downloadprocess_mutex_.lock();
  if (download_process_statistics_[current_thread_id] > now_downloaded) {
    uint64_t key_backup = current_thread_id;
    while (download_process_statistics_.count(key_backup)) {
      std::chrono::milliseconds ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch());
      key_backup = (uint64_t)ms.count();
    }
    download_process_statistics_[key_backup] =
        download_process_statistics_[current_thread_id];
  }
  download_process_statistics_[current_thread_id] = now_downloaded;

  double total_filesize_now_downloaded = 0;
  for (auto i : download_process_statistics_) {
    total_filesize_now_downloaded += i.second;
  }

  if (total_size_to_download_ > 0) {
    double now_percentage =
        total_filesize_now_downloaded * 100 / total_size_to_download_;
    if (int(now_percentage) > total_download_last_percent_) {
      total_download_last_percent_ = int(now_percentage);
      PrintDownloadProgress(total_filesize_now_downloaded,
                            total_size_to_download_);
    }
  }

  downloadprocess_mutex_.unlock();
  return 0;
}

void WebDownloader::PrintDownloadProgress(double now_downloaded,
                                          double total_to_download) {
  LOG(ERROR) << "[download] progress: " << now_downloaded / 1024 << "/"
             << total_to_download / 1024 << " kbytes  "
             << "(" << now_downloaded * 100 / total_to_download << "%)";
  PrintEndl;
}

int WebDownloader::RangeDownloadThreadIns(WebDownloader *pthis,
                                          const std::string &url,
                                          std::shared_ptr<DownloadNode> pnode) {
  return pthis->RangeDownloadThread(url, pnode);
}

int WebDownloader::RangeDownloadThread(
    const std::string &url, std::shared_ptr<DownloadNode> download_node) {
  LOG(ERROR) << "launch download thread id:" << std::this_thread::get_id();
  PrintEndl;
  CURL *curl = curl_easy_init();
  if (NULL == curl) {
    return CURLE_FAILED_INIT;
  }

  char range[64] = {0};
  snprintf(range, sizeof(range), "%ld-%ld", download_node->start_pos,
           download_node->end_pos);

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, MultiThreadWriterCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, download_node.get());
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION,
                   MultiThreadDownloadProgressCallback);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10L);
  curl_easy_setopt(curl, CURLOPT_RANGE, range);

  int res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    LOG(ERROR) << "file range download failed";
    PrintEndl;
    node_manager_mutex_.lock();
    failed_nodes_.emplace_back(download_node);
    node_manager_mutex_.unlock();
    failed_nodes_count_++;
  }
  downloading_thread_count_--;

  curl_easy_cleanup(curl);
  return res;
}

bool WebDownloader::DownloadMission(const int thread_num,
                                    const std::string &url,
                                    const std::string &out_file) {
  total_size_to_download_ = 0;
  total_download_last_percent_ = -1;
  download_process_statistics_.clear();
  failed_nodes_.clear();

  bool download_success = true;

  total_size_to_download_ = GetDownloadFileLenth(url);
  if (total_size_to_download_ <= 0) {
    LOG(ERROR) << "get the file length error...";
    PrintEndl;
    return false;
  }
  FILE *fp = fopen(out_file.c_str(), "wb");
  if (!fp) {
    return false;
  }

  windmill::common::ThreadPool thread_pool(thread_num);

  long part_size = 2 * 1024 * 1024;
  int range_num = int(total_size_to_download_ / part_size) +
                  (total_size_to_download_ % part_size > 1 ? 1 : 0);
  for (int i = 0; i < range_num; i++) {
    auto download_node = std::make_shared<DownloadNode>();
    download_node->start_pos = i * part_size;
    if (i < range_num - 1) {
      download_node->end_pos = (i + 1) * part_size - 1;
    } else if (i == range_num - 1) {
      download_node->end_pos = total_size_to_download_ - 1;
    }
    download_node->fp = fp;
    download_node->reconnect_times = 0;
    downloading_thread_count_++;
    thread_pool.CommitTask(&WebDownloader::RangeDownloadThreadIns, this, url,
                           download_node);
  }

  while ((downloading_thread_count_ > 0 || failed_nodes_count_ > 0) &&
         download_success) {
    // LOG(INFO) << "running nodes num:" << downloading_thread_count_;
    if (failed_nodes_count_ > 0) {
      LOG(ERROR) << "failed nodes num:" << failed_nodes_count_;
      PrintEndl;
      LOG(ERROR) << "reconnect...";
      PrintEndl;
      node_manager_mutex_.lock();
      for (auto &i_node : failed_nodes_) {
        i_node->reconnect_times++;
        LOG(ERROR) << "i_node reconnect times:" << i_node->reconnect_times;
        PrintEndl;
        if (i_node->reconnect_times > max_reconnect_times_) {
          LOG(ERROR) << "failed, reconnect too many times";
          PrintEndl;
          LOG(ERROR) << "download failed! stop other threads!";
          PrintEndl;
          download_success = false;
        } else {
          i_node->fp = fp;
          downloading_thread_count_++;
          thread_pool.CommitTask(&WebDownloader::RangeDownloadThreadIns, this,
                                 url, i_node);
        }
      }
      node_manager_mutex_.unlock();
    }
    node_manager_mutex_.lock();
    failed_nodes_.clear();
    node_manager_mutex_.unlock();
    failed_nodes_count_ = 0;
    std::this_thread::sleep_for(std::chrono::microseconds(800));
  }

  // while(!thread_pool.Finished()){};

  fclose(fp);
  if (!download_success) {
    LOG(ERROR) << "download failed!";
    PrintEndl;
    return false;
  }
  LOG(ERROR) << "download finished.";
  PrintEndl;
  return true;
}

long WebDownloader::total_size_to_download_ = 0;
int WebDownloader::total_download_last_percent_ = -1;
std::unordered_map<uint64_t, double>
    WebDownloader::download_process_statistics_;
std::mutex WebDownloader::downloadprocess_mutex_;
std::mutex WebDownloader::writer_mutex_;

bool DownloadMission(const int thread_num, const std::string &url,
                     const std::string &out_file) {
  WebDownloader downloader;
  return downloader.DownloadMission(thread_num, url, out_file);
}

}  // namespace utility
}  // namespace windmill

int main(int argc, char *argv[]) {
  /* read from argv */
  if (argc != 4) {
    printf("Usage: exe <thread_num> <url> <out_file>\n");
    return 0;
  }

  int thread_num = atoi(argv[1]);
  std::string url = argv[2];
  std::string out_file = argv[3];

  time_t start_time = time(NULL);
  bool result = windmill::utility::DownloadMission(thread_num, url, out_file);
  LOG(ERROR) << "Download Info:"
             << "thread_num:" << thread_num
             << ",result:" << (result ? "successed" : "failed")
             << ",time used(s):" << time(NULL) - start_time;
  PrintEndl;

  return 0;
}
