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
typedef unsigned long long ull;
using namespace std;
typedef struct sockaddr_in  sockaddr_in;
typedef struct list_info{
    string ip;
    string port;
    string mask;	
} list_info;
typedef struct user_info_t{
    string name;
    string password;
    bool status;
    user_info_t(){
        status = false;
        name ="";
        password="";
    }
}user_info;

unordered_map <string,vector<string>> list_table; // filename ->[ip:port, piecewisemask],[ip:port piecewisemask]
unordered_map <string,unordered_map<string,string>> file_mask; // filename -> ip -> filepiece maske
unordered_map <string,ull> file_size;
unordered_map <string,user_info>  user_table;

mutex map_lock;

typedef struct group{
    int id ;
    vector<string> members;
}group;
unordered_map <string,group> validate_table; // filename -> [id,ip:port] join if the requested ip:port isnt in the validate_table[filename]
void check(int status,string msg){
    if(status< 0){
        perror(msg.c_str());
    }
}
vector<string> tokenize(string s){
    vector<string> res;
    string token;
    istringstream ss(s);
    while(ss >> token){
        res.push_back(token);
    }
    return res;
}
string create_list(string req){ // takes filename as argument request should specify the filename
    vector<string> update = tokenize(req);
    if(update.size() < 2){
        return string();
    }
    string ip_port = update[2];
    cout << ip_port << "\n";
    if(user_table.find(ip_port) == user_table.end()){
        return string();
    }
    if(user_table[ip_port].status == false){
        return string();
    }
    string filename = update[1];
    string res2="";
    res2 += '1';
    res2 += '\n';
    for(auto i : file_mask[filename]){
        if(user_table[i.first].status == true){
            res2 = res2 + i.first + ' '+ i.second + ' '+ to_string(file_size[filename]) + '\n';
        }
    }
    res2 += '\0';
    return res2;
}

void update_list(string req){ // takes whole req as argument to update the table
    // ignore first k bits of req yet to be decided
    vector<string> update = tokenize(req);
    if(update.size() < 4){
        return ;
    }
    string filename = update[1];
    string ip = update[2];
    string filepiece = update[3];
    string filesize = update[4];
    cout << "ip is: " <<ip << "\n";
    cout << "filesize is "<< filesize << "\n";
    if(user_table.find(ip) == user_table.end()){
        cout << "user doesnt exist "<<"\n";
        return;
    }
    cout << user_table[ip].status << "\n";
    if(!user_table[ip].status){
        cout << "login before upload/download"<< '\n' ;
        return;
    }
    file_mask[filename][ip] = filepiece;
    file_size[filename] = atoi(filesize.c_str());
    cout << "uploaded succesfully " << "\n";
    return;
}
string create_user(string req)
{
    vector<string> user_string = tokenize(req);
    if(user_string.size() < 5){
        return "can't create user" ;
    }
    user_info record;
    record.name = user_string[3];
    record.password = user_string [4];
    record.status = false;
    string ip_port = user_string[1]+':'+user_string[2];
    if(user_table.find(ip_port) == user_table.end()){
        user_table[ip_port] = record;
        return "user created succesfully";
    }
    return "user already exists";
}
string login_user(string req){
    vector<string> user_string =  tokenize(req);
    if(user_string.size() < 5){
        return "invalid usage of login";
    }
    string ip_port = user_string[1] + ':'+ user_string[2];
    if(user_table.find(ip_port) == user_table.end()){
        return "user doesn't exist";
    }
    if(user_table[ip_port].name == user_string[3] && user_table[ip_port].password == user_string[4]){
        user_table[ip_port].status = true;
        return "logged in succesfully";
    }
    return "invalid credentials";
}
string logout_user(string req){
    vector<string> user_string = tokenize(req);
    if(user_string.size() < 3){
        return "invalid request";
    }
    string ip_port = user_string[1] + ':'+ user_string[2];
    if(user_table.find(ip_port) == user_table.end()){
        return "user doesn't exit";
    }
    user_table.erase(ip_port);
    cout << "logged out succesfully" << "\n";
    return "logged out succesfully";
}
void req_handler(int newfd){
    char req[3072];

    // request format of type "n filename ip:port  filepiece" key is filename
    check(newfd,"bad client");
    explicit_bzero((void*)req,3072);
    read(newfd,req,3072);
    cout << req << "\n";

    // request types
    // type 1 request all the current listing
    // type 2 update the info in tracker map with send information
    string msg = "";

    map_lock.lock(); //lock the table before the operations
    if(req[0] == '1'){
        msg = create_list(string(req));
        cout << "msg from create_list"<< msg << "\n";
        send(newfd,msg.c_str(),msg.length(),0);
    }
    // update table request with data sent in read;
    else if(req[0] == '2'){
        cout << "msg recieved" << "\n";
        msg = "acknowledged";
        update_list(string(req));
        send(newfd,msg.c_str(),msg.length(),0);		
    }
    else if(req[0]== '3'){
        string msg = create_user(req);
        cout << msg << "\n";
        send(newfd,msg.c_str(),msg.length(),0);
    }
    else if(req[0] == '4'){
        string msg = login_user(req);
        cout << msg << "\n";
        send(newfd,msg.c_str(),msg.length(),0);
    }
    else if(req[0] == '5'){
        string msg = logout_user(req);
        cout << msg << "\n";
        send(newfd,msg.c_str(),msg.length(),0);
    }
    close(newfd); //close the table before the opearations
    map_lock.unlock();
}
int main(int argc, char** argv){
    if(argc < 2){
        cout << "invalid usage "<< "\n";
        return 0 ;
    }
    int fd1 =  open(argv[1],O_RDONLY);
    struct stat file_info;
    stat(argv[1],&file_info);
    size_t nbytes = file_info.st_size;
    char buffer[nbytes] ;
    read(fd1,buffer,nbytes);
    stringstream s(buffer);
    string tmp;
    vector<string> final;
    while(getline(s,tmp,'\n')){
        final.push_back(tmp);
    }
    string ip_port_tracker = final[0];
    cout << ip_port_tracker << "\n";
    int pos_2 = ip_port_tracker.find(':');
    string tracker_ip  = ip_port_tracker.substr(0,pos_2);
    string tracker_port = ip_port_tracker.substr(pos_2+ 1) ;


    // get the port info from cmd line arguments after wards
    int sockfd,portno;
    socklen_t client;
    sockaddr_in server_addr , client_addr;
    string msg = "hello world from server";
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    explicit_bzero((void *)&server_addr, sizeof(server_addr));
    portno = atoi(tracker_port.c_str());
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(tracker_ip.c_str());
    server_addr.sin_port = htons(portno);
    check(bind(sockfd,(sockaddr *)&server_addr,sizeof(server_addr)),"binding failed");
    while(true){
        listen(sockfd,5);
        int newfd = accept(sockfd,NULL,NULL);
        char addr_buff[50];
        thread t1(req_handler,newfd);
        t1.detach();
    }
    close(sockfd);
}
