[中文说明](https://github.com/YuYuCong/Downloader/blob/main/readme-zh.md)

# WindmillDownloader

WindmillDownloader is a web downloader.

#### Feature

- Thread pool.
- MultiThread Download.
- Break point reconnect.

#### Platform

Ubuntu.

#### Dependencies

- curl
- thread
- glog
- gflags
- boost (filesystem)

1. curl

```shell
$ sudo apt-get install curl
```

2. boost

```shell
# 下载boost库 地址 https://www.boost.org/
$ tar -zxvf ./boost.tar.gz
$ cd ./boost_1_70_0
$ sudo ./bootstrap.sh
$ sudo ./b2 install
```

3. glog

```shell
$ sudo apt-get install libgoogle-glog-dev
```

4. gflags

```shell
$ git clone https://github.com/gflags/gflags.git
$ cd gflags
$ make build
$ cd build
$ cmake ..
$ make -j12
$ make install
```

#### Build

```shell
$ mkdir build
$ cd build
$ cmake ..
$ make -j
```

#### Usage

```shell
$ ./windmill_downloader [--thread_num <thread_num>] --url <url> [--out_file <output_file>]
# example:
$ ./windmill_downloader --thread_num 32 --url https://releases.ubuntu.com/20.04/ubuntu-20.04.3-desktop-amd64.iso --out_file ./ubuntu-20.04.3-desktop-amd64.iso
$ ./windmill_downloader --url http://www.gecif.net/articles/mathematiques/pi/pi_1_million.txt --out_file pi_1_million.txt
```

#### Contact

Feel free to contact me [windmillyucong@163.com](mailto:windmillyucong@163.com)
anytime for anything.

#### License

Copyleft! 2021 William Yu. Some rights reserved.

[![CC0](http://i.creativecommons.org/p/zero/1.0/88x31.png)](http://creativecommons.org/publicdomain/zero/1.0/)

