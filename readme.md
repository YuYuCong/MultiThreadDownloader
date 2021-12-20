# WindmillDownloader 
WindmillDownloader is a web downloader.

#### Feature

- Thread pool.
- MultiThread Download.
- Break point reconnect. 

#### Platform
Ubuntu 16.04 or Ubuntu 18.04

#### Dependencies
- curl 
- thread
```
sudo apt-get install curl
```

#### Build
```shell
g++ ./windmill_downloader.cpp -o downloader -lcurl -lpthread -std=c++14
```

#### Usage
```shell
./downloader <thread_num> <url> <output_file>
# example:
./downloader 32 https://releases.ubuntu.com/20.04/ubuntu-20.04.3-desktop-amd64.iso ./ubuntu-20.04.3-desktop-amd64.iso
```

#### Contact

Feel free to contact me [windmillyucong@163.com](mailto:windmillyucong@163.com) anytime for anything.


#### License

Copyleft! 2021 William Yu. Some rights reserved.

[![CC0](http://i.creativecommons.org/p/zero/1.0/88x31.png)](http://creativecommons.org/publicdomain/zero/1.0/)

