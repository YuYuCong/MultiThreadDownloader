# WindmillDownloader

这是一个多线程下载器.

#### 性能

- 线程池.
- 多线程下载.
- 断点续传.

#### 平台

Ubuntu

#### 依赖安装

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

#### 编译

```shell
$ mkdir build
$ cd build
$ cmake ..
$ make -j
```

#### 使用

```shell
$ ./windmill_downloader [--thread_num <thread_num>] --url <url> [--out_file <output_file>]
# 例子:
$ ./windmill_downloader --thread_num 32 --url https://releases.ubuntu.com/20.04/ubuntu-20.04.3-desktop-amd64.iso --out_file ./ubuntu-20.04.3-desktop-amd64.iso
$ ./windmill_downloader --url http://www.gecif.net/articles/mathematiques/pi/pi_1_million.txt --out_file pi_1_million.txt
```

#### 联系

欢迎邮件联系 [windmillyucong@163.com](mailto:windmillyucong@163.com).

#### License

Copyleft! 2021 William Yu. Some rights reserved.

[![CC0](http://i.creativecommons.org/p/zero/1.0/88x31.png)](http://creativecommons.org/publicdomain/zero/1.0/)

