#include<iostream>
#include <sstream>
#include<unordered_map>
#include <cstring>
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
#include<vector>

#define BLK   512

typedef struct list_info{
    std::string ip;
    std::string port;
    std::string mask;	
} list_info;

typedef struct user_info_t{
    std::string name;
    std::string password;
    bool status;
    user_info_t(){
        status = false;
        name ="";
        password="";
    }
}user_info;

typedef struct group{
    int id ;
    std::vector<std::string> members;
}group;
