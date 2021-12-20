#include <boost/filesystem.hpp>
#include <glog/logging.h>
#include <iostream>
#include <thread>

#include "downloader/downloader.h"
#include "threadpool/threadpool.h"

namespace {
const int kMaxAllowedReconnectTimes = 15;
const std::string kCurlCaInfo = "../app/resource/cacert.pem";
}  // namespace

namespace windmill {
WindmillDownloader::WindmillDownloader()
    : remaining_tasks_count_(0),
      max_reconnect_times_(kMaxAllowedReconnectTimes) {}

WindmillDownloader::~WindmillDownloader() = default;

long WindmillDownloader::GetDownloadFileSize(const std::string &url) {
  long download_file_size = -1;
  CURL *curl = curl_easy_init();
  if (nullptr == curl) {
    return -1;
  }

  curl_off_t cl;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HEADER, 1);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
  if (boost::filesystem::exists(kCurlCaInfo)) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, kCurlCaInfo.c_str());
  } else {
    LOG(ERROR) << "curl ca file not exist, disable ssl";
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 20L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
  if (CURLE_OK == curl_easy_perform(curl)) {
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &cl);
    download_file_size = long(cl);
  } else {
    LOG(ERROR) << "file size check failed!";
  }
  curl_easy_cleanup(curl);
  LOG(ERROR) << "download_file_size: " << download_file_size;
  return download_file_size;
}

size_t WindmillDownloader::MultiThreadWriterCallback(void *ptr, size_t size,
                                                     size_t nmemb,
                                                     void *userdata) {
  auto *node = (DownloadNode *)userdata;
  size_t written;
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

int WindmillDownloader::MultiThreadDownloadProgressCallback(
    void *ptr, double total_to_download, double now_downloaded,
    double total_to_upload, double now_uploaded) {
  auto current_thread_id = pthread_self();

  download_process_mutex_.lock();
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

  download_process_mutex_.unlock();
  return 0;
}

void WindmillDownloader::PrintDownloadProgress(double now_downloaded,
                                               double total_to_download) {
  LOG(ERROR) << "[download] progress: " << now_downloaded / 1024 << "/"
             << total_to_download / 1024 << " kbytes  "
             << "(" << now_downloaded * 100 / total_to_download << "%)";
}

int WindmillDownloader::RangeDownloadThreadIns(
    WindmillDownloader *pthis, const std::string &url,
    const std::shared_ptr<DownloadNode> &pnode) {
  return pthis->RangeDownloadThread(url, pnode);
}

int WindmillDownloader::RangeDownloadThread(
    const std::string &url,
    const std::shared_ptr<DownloadNode> &download_node) {
  LOG(ERROR) << "launch download thread id:" << std::this_thread::get_id();
  CURL *curl = curl_easy_init();
  if (nullptr == curl) {
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
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10L);
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 20L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
  curl_easy_setopt(curl, CURLOPT_RANGE, range);

  int res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    LOG(ERROR) << "file range download failed";
    node_manager_mutex_.lock();
    failed_nodes_.emplace_back(download_node);
    node_manager_mutex_.unlock();
  }
  remaining_tasks_count_--;

  curl_easy_cleanup(curl);
  return res;
}

bool WindmillDownloader::DownloadMission(const int thread_num,
                                         const std::string &url,
                                         const std::string &out_file) {
  total_size_to_download_ = 0;
  total_download_last_percent_ = -1;
  download_process_statistics_.clear();

  node_manager_mutex_.lock();
  failed_nodes_.clear();
  node_manager_mutex_.unlock();

  total_size_to_download_ = GetDownloadFileSize(url);
  if (total_size_to_download_ <= 0) {
    LOG(ERROR) << "get file size error...";
    return false;
  }
  FILE *fp = fopen(out_file.c_str(), "wb");
  if (!fp) {
    return false;
  }

  windmill::ThreadPool thread_pool(thread_num);

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
    remaining_tasks_count_++;
    thread_pool.CommitTask(&WindmillDownloader::RangeDownloadThreadIns, this,
                           url, download_node);
  }

  bool downloading = true;
  bool download_success = false;
  while (downloading) {
    node_manager_mutex_.lock();
    if (!failed_nodes_.empty()) {
      LOG(ERROR) << "failed nodes num:" << failed_nodes_.size();
      LOG(ERROR) << "reconnect...";
      for (auto &i_node : failed_nodes_) {
        i_node->reconnect_times++;
        LOG(ERROR) << "i_node reconnect times:" << i_node->reconnect_times;
        if (i_node->reconnect_times > max_reconnect_times_) {
          LOG(ERROR) << "failed, reconnect too many times";
          LOG(ERROR) << "download failed! stop other threads!";
          downloading = false;
          download_success = false;
          break;
        } else {
          i_node->fp = fp;
          remaining_tasks_count_++;
          thread_pool.CommitTask(&WindmillDownloader::RangeDownloadThreadIns,
                                 this, url, i_node);
        }
      }
      failed_nodes_.clear();
    }
    node_manager_mutex_.unlock();

//    auto thread_pool_top = thread_pool.GetTop();
//    LOG(INFO) << "current_workers_num:" << thread_pool_top->current_workers_num;
//    LOG(INFO) << "current_running_tasks_num:"
//              << thread_pool_top->current_running_tasks_num;
//    LOG(INFO) << "current_remaining_tasks_num:"
//              << thread_pool_top->current_remaining_tasks_num;
//    LOG(INFO) << "total_processed_tasks_num:"
//              << thread_pool_top->total_processed_tasks_num;
    if (remaining_tasks_count_ > 0) {
 //   LOG(INFO) << "remaining download tasks num:" << remaining_tasks_count_;
      downloading = true;
    } else {
      downloading = false;
      download_success = true;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(800));
  }

  // while(!thread_pool.Finished()){};

  fclose(fp);
  if (!download_success) {
    LOG(ERROR) << "download failed!";
    return false;
  }
  LOG(ERROR) << "download finished.";
  return true;
}

long WindmillDownloader::total_size_to_download_ = 0;
int WindmillDownloader::total_download_last_percent_ = -1;
std::unordered_map<uint64_t, double>
    WindmillDownloader::download_process_statistics_;
std::mutex WindmillDownloader::download_process_mutex_;
std::mutex WindmillDownloader::writer_mutex_;

}  // namespace windmill