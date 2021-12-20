#include <glog/logging.h>
#include <iostream>
#include <string>

#include "downloader/downloader.h"

namespace windmill {
bool DownloadMission(const int thread_num, const std::string &url,
                     const std::string &out_file) {
  WindmillDownloader downloader;
  return downloader.DownloadMission(thread_num, url, out_file);
}
}  // namespace windmill

int main(int argc, char *argv[]) {
  /* read from argv */
  if (argc != 4) {
    LOG(ERROR) << "Usage: exe <thread_num> <url> <out_file>";
    return 0;
  }

  const int thread_num = atoi(argv[1]);
  const std::string url = argv[2];
  const std::string out_file = argv[3];

  const time_t start_time = time(NULL);
  const bool result = windmill::DownloadMission(thread_num, url, out_file);
  LOG(ERROR) << "Download Info:"
             << "thread_num:" << thread_num
             << ",result:" << (result ? "successed" : "failed")
             << ",time used(s):" << time(NULL) - start_time;

  return 0;
}
