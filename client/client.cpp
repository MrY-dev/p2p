#include<atomic>
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
std::mutex tmp_lock;
using namespace std;
string server_ip;
string server_port;
string tracker_ip;
string tracker_port;

typedef struct sockaddr_in  sockaddr_in;
//typedef struct hostent  hostent;
sockaddr_in tracker;
typedef struct ip_info{
    string ip;
    uint port;
    string mask;
    ull filesize;
}ip_info;

void check (int x,string msg=""){
    if(x < 0){
        cout << msg << "\n";
    }
}
typedef struct file_info{
    string filepiece;
    string filehash;
    ull size;
} file_info;

vector<string> tokenizer(string one){
    vector<string> res;
    if(one.empty()){
        return res;
    }
    istringstream tok(one);
    string tmp;
    while(tok >> tmp){
        res.push_back(tmp);
    }
    return res;
}


unordered_map<string,file_info> curr_file_info; // used by the client of client for storing
unordered_map<string,string> file_path ; // used by the serverside of client for storing file path info
unordered_map<string,ull> file_size;
//send the piece of file requested request should be of type filename and piece
unordered_map<string,vector<string>> piece_hash_info; // used by the server to send hashes of the requested by pieces



pair<string,int> genpiece(string path){
    pair<string,int> res;
    res.first = "";
    struct stat file_stat;
    int l = stat(path.c_str(),&file_stat);
    if(l == -1){
        open(path.c_str(),O_CREAT|O_RDONLY);
    }
    stat(path.c_str(),&file_stat);
    size_t filesize = file_stat.st_size;
    cout << "filesize is " <<  filesize << "\n";
    int len = ceil((float)filesize/CHUNK);
    res.second = filesize;
    string filename = path.substr(path.find_last_of('/')+1);

    cout << filename << "\n";
    file_size[filename] = filesize;
    for(int i = 0; i < len;++i){
        res.first += '1';
    }
    curr_file_info[filename].filepiece = res.first;
    curr_file_info[filename].size = filesize;
    return res;
}

void handle_req(int reqfd){
    // file request handler send filename
    //char buffer[sub_size];//
    char* buffer = new char[SUBCHUNK];
    char req[BUFF_SIZE];
    explicit_bzero((void*)req,BUFF_SIZE);
    explicit_bzero((void*)buffer,SUBCHUNK);

    read(reqfd,req,BUFF_SIZE);
    cout << "request recieved is " << req  << "\n";

    string request = string(req); // of the format filename piece subpiece;
    if(req[0] == '1'){
        vector<string> file_upload = tokenizer(request);
        string filename = file_upload[1];
        int piece = atoi(file_upload[2].c_str());
        int subpiece = atoi(file_upload[3].c_str());
        //
        int upfd  = open(file_path[filename].c_str(),O_RDONLY);
        lseek(upfd,(piece-1)*CHUNK,SEEK_SET);
        lseek(upfd,(subpiece-1)*SUBCHUNK,SEEK_CUR);
        read(upfd,buffer,SUBCHUNK);
        int l = send(reqfd,buffer,SUBCHUNK,MSG_NOSIGNAL);
        delete[] buffer;
        cout << l << " bytes sent" << "\n";
        close(upfd);
    }
    else if(req[0] == '2'){// hash verification for piece request
        vector<string> file_hash = tokenizer(request); // request for hash is 2 filename and piece
        string filename = file_hash[1];
        int piece = atoi(file_hash[2].c_str());
        //string res = piece_hash_info[filename][piece]; // indexing starts at 0
        size_t len = curr_file_info[filename].filepiece.length();
        size_t filesize = curr_file_info[filename].size;
        int readfd  = open(file_path[filename].c_str(),O_RDONLY);

        if(piece+1 == len){
            char* buffer = new char[filesize%CHUNK];
            lseek(readfd,(piece)*CHUNK,SEEK_SET);
            unsigned char hash[20];
            read(readfd,buffer,filesize%CHUNK);
            SHA1((const unsigned char*)buffer, (filesize%CHUNK) -1 ,(unsigned char*)hash);
            std::ostringstream os;
            os.fill('0');
            os<<std::hex;
            for(const unsigned char *ptr = hash; ptr < hash+20; ++ptr) {
                os<<setw(2)<<(unsigned int)*ptr;
            }
            string res = os.str();
            res += '\0';
            send(reqfd,res.c_str(),res.length(),MSG_NOSIGNAL);
            close(readfd);
            delete[] buffer;
        }
        else{
            char* buffer = new char[CHUNK];
            lseek(readfd,(piece)*CHUNK,SEEK_SET);
            unsigned char hash[20];
            read(readfd,buffer,CHUNK);
            SHA1((const unsigned char*)buffer,CHUNK-1,(unsigned char*)hash);
            //string res = string(hash);
            std::ostringstream os;
            os.fill('0');
            os<<std::hex;
            for(const unsigned char *ptr = hash; ptr < hash+20; ++ptr) {
                os<<setw(2)<<(unsigned int)*ptr;
            }
            string res = os.str();
            res += '\0';
            send(reqfd,res.c_str(),res.length(),MSG_NOSIGNAL);
            close(readfd);
            delete[] buffer;
        }
    }
    close(reqfd);
    return;
}

void serve(string ip ,int port){
    sockaddr_in cli;
    explicit_bzero((void*)&cli, sizeof(cli));
    cli.sin_port =htons(port);
    cli.sin_addr.s_addr = inet_addr(ip.c_str());
    cli.sin_family = AF_INET;
    int servefd = socket(AF_INET,SOCK_STREAM,0);
    check(bind(servefd, ( sockaddr *)&cli, sizeof(cli)),"failed binding");
    // read the file
    // request should be like filename piecenum
    while(true){
        listen(servefd,10);
        int newfd = accept(servefd,  (sockaddr *)NULL, NULL);
        thread t3(handle_req,newfd);
        t3.detach();
    }
    return;
}
// select piecewise generte requests 
void conv(sockaddr_in* res,ip_info* x){
    res->sin_family = AF_INET;
    res->sin_port = htons(x->port);
    res->sin_addr.s_addr = inet_addr(x->ip.c_str());

}
void writedown(sockaddr_in* ip, string path, string filename,int curr_piece, int len,int sub_piece,int wrfd){
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    int l = connect(sockfd,(sockaddr*)ip,sizeof(*ip));
    string req = "1 "+ filename + " "+ to_string(curr_piece) + " " + to_string(sub_piece);
    send(sockfd,req.c_str(),req.length(),0);


    char* read_buff = new char[SUBCHUNK];

    recv(sockfd,read_buff,SUBCHUNK,MSG_WAITALL);
    //cout << "currently outputting to stdout  " << read_buff << "\n";
    close(sockfd);
    // write to the file using offset no of bytes
    lseek(wrfd,(curr_piece-1)*CHUNK+(sub_piece-1)*SUBCHUNK,SEEK_SET);
    write(wrfd,read_buff,SUBCHUNK);
    delete[] read_buff;
    return ;
}


string gethash(ip_info ip,int piece,string filename){
    sockaddr_in verify_ip;
    conv(&verify_ip,&ip);
    string req = "2 " + filename+" "+to_string(piece) ;
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    int err = connect(sockfd,(sockaddr*)&verify_ip,sizeof(verify_ip));
    send(sockfd,req.c_str(),req.length(),0);
    char read_buff[51];
    read(sockfd,read_buff,51);
    string hashbuff = string(read_buff);
    close(sockfd);
    return hashbuff;
}
void update_tracker(string filename, string mask,uint filesize){
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    string req = "2 "+ filename+ " "+ server_ip+':'+server_port+" "+ mask+ " "+to_string(filesize);
    int err = connect(sockfd,(sockaddr*)&tracker,sizeof(tracker));
    send(sockfd,req.c_str(),req.length(),0);
    char tmpbuff[BUFF_SIZE];
    read(sockfd,tmpbuff,BUFF_SIZE);
    close(sockfd);
    return;
}
string getcurrhash(int fd, size_t curr_piece,size_t filesize,size_t len){
    lseek(fd,curr_piece*CHUNK,SEEK_SET);
    if(curr_piece +1 == len){
        char* buffer2 = new char[filesize%CHUNK];
        read(fd,buffer2,filesize%CHUNK);
        unsigned char hash[20];
        SHA1((const unsigned char*)buffer2,(filesize%CHUNK) -1 ,(unsigned char*)hash);
        std::ostringstream os;
        os.fill('0');
        os<<std::hex;
        for(const unsigned char *ptr = hash; ptr < hash+20; ++ptr) {
            os<<setw(2)<<(unsigned int)*ptr;
        }
        delete [] buffer2;
        return os.str();
    }
    else{
        char* buffer = new char[CHUNK];
        read(fd,buffer,CHUNK);
        unsigned char hash[20];
        SHA1((const unsigned char*)buffer,CHUNK-1,(unsigned char*)hash);
        std::ostringstream os;
        os.fill('0');
        os<<std::hex;
        for(const unsigned char *ptr = hash; ptr < hash+20; ++ptr) {
            os<<setw(2)<<(unsigned int)*ptr;
        }
        delete [] buffer;
        return os.str();
    }
}
// void verify_file_hash(int wrfd,ip_info * ip){
// 	conv(ip)
// }
void download_piece(const vector<int>& missing,const vector<ip_info>& ips,int curr_piece,string filename, string path,size_t len, int wrfd,size_t filesize){
    string orighash = "1";
    string currhash = "0";
    if(curr_piece > len){
        return;
    }
    curr_piece = missing[curr_piece-1];
    while(orighash != currhash ){
        vector<ip_info> final_ips;
        for(auto i : ips){
            if( i.mask[curr_piece-1] == '1' && i.ip+to_string(i.port) != server_ip+server_port){
                final_ips.push_back(i);
            }
        }
        sockaddr_in  curr_ip;
        explicit_bzero(&curr_ip, sizeof(curr_ip));
        for(int i = 1; i <= 16 ;++i){
            conv(&curr_ip,&final_ips[i%(final_ips.size())]); 
            writedown(&curr_ip,path,filename,curr_piece,len,i,wrfd);
        }
        if(curr_piece == len){
            ftruncate(wrfd,filesize);
        }
        orighash = gethash(final_ips[0],curr_piece-1,filename);
        currhash = getcurrhash(wrfd,curr_piece -1,filesize,len);
        cout << orighash << "\n";
        cout << currhash << "\n";
        cout << "piece: " << curr_piece << " " << boolalpha << (currhash == orighash) << "\n";
    }
    cout << "this piece no is fine " << curr_piece <<  "\n";
    curr_file_info[filename].filepiece[curr_piece-1]   = '1' ;
    update_tracker(filename, curr_file_info[filename].filepiece,  filesize);
}
void download(vector<ip_info> ips,string filename,string path){
    cout << "entering download area" << "\n";
    uint filesize = ips[0].filesize;
    uint len = ceil((float)filesize/(CHUNK));
    string tmp = "" ;
    string full = "";
    for(int i = 0; i < len;++i){
        tmp += '0';
        full += '1';
    }
    if(curr_file_info[filename].filepiece == full){
        cout << "already downloaded";
        return ;
    }
    curr_file_info[filename].filepiece = tmp;
    (piece_hash_info[filename]).reserve(len+1);
    int wrfd = open(path.c_str(),O_CREAT|O_RDWR,0666);
    //download the complete file before
    cout << "currently downloading filename: " << filename << "\n";
    size_t total  = ceil((float)filesize/CHUNK);
    vector<int> missing_pieces ;
    cout<< "total pieces is " << total  << "\n";
    for(int i = 1 ; i < total+1; ++i){
        if((curr_file_info[filename].filepiece[i-1]) == '0' ){
            missing_pieces.push_back(i);
        }
    }
    auto rng = default_random_engine{};
    shuffle(begin(missing_pieces),end(missing_pieces) , rng);
    for(size_t j = 1; j < len ; j+=4){
        for(int i = 0; i < 4; ++i){
            vector<thread> piece_thread;
            piece_thread.push_back(thread(download_piece,missing_pieces,ips,i+j,filename,path,len,wrfd,filesize));
            for(auto &th  : piece_thread){
                th.join();
            }
        }
    }
    // for(auto i :missing_pieces){
    // 	download_piece(ips,i,filename,path,len,wrfd,filesize);
    // }
    // for(auto i : missing_pieces){
    // 	piece_thread.push_back(thread(download_piece, ips, i,  filename,  path,  len,  wrfd,  filesize));
    // 	//download_piece(ips,i,filename,path,len,wrfd,filesize);
    // }
    // for(auto &th: piece_thread){
    // 	th.join();
    // }
    cout << curr_file_info[filename].filepiece << "\n";
    cout << "the file has been downloaded" << "\n";
    close(wrfd);
}
void req_handler(pair<string,string> req){
    string request = req.second;
    string path = req.first; // genearte filename and path from here
    vector<string> res1 = tokenizer(request);
    vector<string> res2 = tokenizer(path);
    if(res1.size() == 0){
        return;
    }

    sockaddr_in serv;
    explicit_bzero((void*)&serv, sizeof(serv));
    //tracker ip and tracker port no
    serv.sin_port = htons(atoi(tracker_port.c_str()));
    serv.sin_addr.s_addr = inet_addr(tracker_ip.c_str());
    serv.sin_family = AF_INET;

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    int x = connect(sockfd,(sockaddr*)&serv,sizeof(serv));

    string tmp;	
    char inbuffer[1000];
    send(sockfd,request.c_str(),request.length(),0);
    read(sockfd,inbuffer,1000);
    if(inbuffer[0]=='1'){
        cout << "entering the req_handler for download" << "\n";
        string data = string(inbuffer);
        cout << data << "\n";
        close(sockfd);
        stringstream s(data);
        string tmp;
        vector<ip_info> ips ;
        // extract the ip,port,filepieces, info from the data
        while(getline(s,tmp,'\n')){
            if(tmp != "1"){
                cout << "data from the tracker "<< tmp << "\n";
                vector<string> data2 = tokenizer(tmp);
                cout << "ip:port info " << data2[0] << "\n";
                cout << "mask is " << data2[1] << "\n";
                cout << "filesize is" << data2[2] << "\n";
                string ip = data2[0].substr(0,data2[0].find(':'));
                string strport = data2[0].substr(data2[0].find(':')+1);
                uint port = atoi(strport.c_str());
                uint filesize = atoi(data2[2].c_str());
                string mask = data2[1];
                ip_info x = {ip,port,mask, filesize};
                ips.push_back(x);
            }
        }
        string  filename = path.substr(path.find_last_of('/')+1);
        download(ips,filename,path);
    }
}

pair<string,string> parse(string command){
    string req="";
    pair<string,string> def = make_pair(string(),string());
    vector<string> comm = tokenizer(command);
    string filename = comm[1];
    int len = comm[1].length();
    if(comm[1].find('/') != string::npos){
        int pos = comm[1].find_last_of('/');		
        filename = comm[1].substr(pos+1);
        cout << filename.length() << "\n";
    }

    if(comm[0] == "download"){
        req += "1 "+filename+" "+server_ip + ":" + server_port + '\0';
        pair<string,string> res2 = make_pair(comm[1],req);
        return res2;
    }
    else if(comm[0] == "upload"){
        pair<string,int> res = genpiece(comm[1]);
        file_path[filename] = comm[1];
        req += "2 "+filename+" "+server_ip+":"+server_port + " "+ res.first + " "+ to_string(res.second);
        pair<string,string> res2 = make_pair(comm[1],req);
        return res2;
    }
    else if(comm[0] == "create_user"){
        if(comm.size() < 3){
            cout << "incorrect usage" << "\n";
            return def;
        }
        req += "3 "+ server_ip +" "+server_port+" "+comm[1]+" "+ comm[2];
        pair<string,string> res3 = make_pair(comm[1],req);

        return res3;
    }
    else if(comm[0] == "login"){
        if(comm.size() < 3){
            cout << "incorrect usage " << "\n";
            return def;
        }
        req += "4 "+ server_ip +" "+server_port+" "+comm[1]+" "+ comm[2];
        pair<string,string> res4 = make_pair(comm[1],req);

        return res4;
    }
    // else if(comm[0] == "logout"){
    // 	req += "5 "+ server_ip + " "+ server_port+'\0';
    // 	pair<string,string> res5 = make_pair(comm[0],req);
    // 	return res5;
    // }
    return def;
}


int main(int argc, char** argv){
    if(argc < 3){
        cout << "usage ./client <IP>:<PORT> tracker_info.txt" << "\n";
        return 1;
    }
    // retrieve tracker info from 3rd argument;
    int fd1 =  open(argv[2],O_RDONLY);
    struct stat file_info;
    stat(argv[2],&file_info);
    size_t nbytes = file_info.st_size;
    char buffer[nbytes] ;
    read(fd1,buffer,nbytes);
    cout << buffer << "\n";
    close(fd1);
    stringstream s(buffer);
    string tmp;
    vector<string> final;
    while(getline(s,tmp,'\n')){
        final.push_back(tmp);
    }
    string ip_port_tracker = final[0];
    int pos_2 = ip_port_tracker.find(':');
    tracker_ip  = ip_port_tracker.substr(0,pos_2);
    tracker_port = ip_port_tracker.substr(pos_2+ 1) ;
    string ip_port = argv[1];
    int pos =ip_port.find(':');
    string ip = ip_port.substr(0,pos);
    uint port = atoi(ip_port.substr(pos+1).c_str());
    cout << "ip is:" <<tracker_ip<<"\n" ;
    cout << "port is :"<< tracker_port << "\n";
    server_ip = ip;
    server_port = to_string(port);
    tracker.sin_port = htons(atoi(tracker_port.c_str()));
    tracker.sin_addr.s_addr = inet_addr(tracker_ip.c_str());
    tracker.sin_family = AF_INET;

    thread t1(serve,ip,port);
    while(true){
        string in ;
        cout << "$ :";
        getline(cin,in);
        if (in == "quit") {
            exit(0);
        }		
        cout << in << "\n";
        thread t2(req_handler,parse(in));
        t2.join();
    }
    t1.join();
}
