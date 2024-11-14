#include<algorithm>
#include<random>
#include<iomanip>
#include <cmath>
#include <cstddef>
#include<openssl/sha.h>
#include <cstdio>
#include<vector>
#include<sstream>
#include <unordered_map>
#include<iostream>
#include <cstring>
#include <strings.h>
#include<thread>
#include<arpa/inet.h>
#include<string>
#include<sys/socket.h>
#include<sys/types.h>
#include<fcntl.h>
#include<netdb.h>
#include<netinet/in.h>
#include<unistd.h>
#include<sys/stat.h>
#include<mutex>
// buff size for communication
#define BUFF_SIZE 2048
// piece of size 512kb
#define CHUNK 524288
// subpiece of size 32kb
#define SUBCHUNK 32768


typedef unsigned long long  ull;

typedef struct file_info{
    std::string filepiece;
    std::string filehash;
    ull size;
} file_info;

typedef struct ip_info{
    std::string ip;
    uint port;
    std::string mask;
    ull filesize;
}ip_info;

