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

DEFINE_int32(thread_num, 8, "thread num");
DEFINE_string(url,
              "http://www.gecif.net/articles/mathematiques/pi/pi_1_million.txt",
              "file url");
DEFINE_string(out_file, "./pi_1_million.txt", "out file");

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  std::stringstream ss;
  ss << std::endl << "\033[1m";
  ss << argv[0];
  ss << " [--thread_num <thread_num>]";
  ss << " --url <file url>";
  ss << " [--out_file <out file>]";
  ss << std::endl << "\033[0m" << std::endl;
  google::SetUsageMessage(ss.str());
  google::ParseCommandLineFlags(&argc, &argv, false);

  const int thread_num = FLAGS_thread_num;
  const std::string url = FLAGS_url;
  const std::string out_file = FLAGS_out_file;
  LOG(ERROR) << "Download Info:"
             << "thread_num:" << thread_num << ", file_url:" << url
             << ", out_file:" << out_file;

  const time_t start_time = time(NULL);
  const bool result = windmill::DownloadMission(thread_num, url, out_file);
  LOG(ERROR) << "Download Info:"
             << "thread_num:" << thread_num
             << ",result:" << (result ? "completed" : "failed")
             << ",time used(s):" << time(NULL) - start_time;

  return 0;
}
