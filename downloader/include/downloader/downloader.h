#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <curl/curl.h>
#include <pthread.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace windmill {

/**
 * Define a WindmillDownloader.
 */
class WindmillDownloader {
 public:
  WindmillDownloader();
  ~WindmillDownloader();
  bool DownloadMission(const int thread_num, const std::string &url,
                       const std::string &out_file);

 private:
  struct DownloadNode {
    FILE *fp;
    long start_pos;
    long end_pos;
    int reconnect_times;
  };

  long GetDownloadFileSize(const std::string url);

  static size_t MultiThreadWriterCallback(void *ptr, size_t size, size_t nmemb,
                                          void *userdata);

  static int MultiThreadDownloadProgressCallback(void *ptr,
                                                 double total_to_download,
                                                 double now_downloaded,
                                                 double total_to_upload,
                                                 double now_uploaded);

  static void PrintDownloadProgress(double now_downloaded,
                                    double total_to_download);

  static int RangeDownloadThreadIns(WindmillDownloader *pthis,
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
}  // namespace windmill

#endif
