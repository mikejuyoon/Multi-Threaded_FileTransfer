/*
* First Name: Ju Yong
* Last Name: Yoon
* Email address: mikejuyoon@gmail.com
*
* Client that requests and receives files from servers
* File: mtServer.cpp
*
* I hereby certify that the contents of this file represent
* my own original individual work.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <vector>
#include <sstream>
#include <sys/types.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <sys/sendfile.h>
#include <pthread.h>

using namespace std;

const int MAX_THREADS = 10;
const char* COPY_FOLD = "./cpyfiles/";

//Vector of arguments
vector<string> argVec;

//Writes data in socket into an open fd
void sockToFile(int sockfd, int openfd);

//parses path to get name of last item in path.
//returns dynamically allocated char[]
char* getFileName( char* path );

//format the new Thread#file path
//returns dynamically allocated char[]
char* setCurrThreadPath(int threadid);

//main driver for threads
void *sendFilesFunc(void *threadinput);

int main(int argc, char* argv[]){
    if(argc < 3){
        perror("Client- Invalid number of arguments!");
        exit(1);
    }
    for(int i = 0 ; i < argc ; i++ ){
        argVec.push_back(argv[i]);
    }

    //============ Create Threads ============
    pthread_t threads[MAX_THREADS];
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    void *status;

    //============ Make new copy directory ============
    if(mkdir("./cpyfiles",0777))
        perror("./cpyfiles");

    //============ Create defined thread amount ============
    for( int t = 0 ; t < MAX_THREADS ; t++ ){
        if( pthread_create(&threads[t], &attr, sendFilesFunc, (void*)t) )
        {
            cout << "Thread creation failed." << endl;
            exit(-1);
        }
    }

    //============ Join Threads for Sync. ============
    pthread_attr_destroy(&attr);
    for( int t = 0 ; t < MAX_THREADS ; t++ ){
        if ( pthread_join(threads[t], &status) ){
            cout << "ERROR: pthread_join()" << endl;
            exit(-1);
        }
    }

    pthread_exit(NULL);
	return 0;
}

//*************** FUNCTIONS **************
void sockToFile(int sockfd, int openfd){
    char buffer[256];

    //============ Writes exact amount of bytes sent ============
    int writeSize;
    do{
        bzero(buffer,256);
        if(( writeSize = read(sockfd, buffer, 255) ) <=0 )
            return;
        write(openfd, buffer, writeSize);
    }while(writeSize == 255);
}

char* getFileName( char* path ){
    int i = 0;
    int lastSlash = -1;
    //============ Finds the last '/' ============
    while(path[i] != '\0'){
        if(path[i] == '/' && path[i+1] != '\0')
            lastSlash = i;
        i++;
    }
    if(lastSlash == -1)
        return path;

    char *fileName = new char[80];
    bzero(fileName,80);

    i = lastSlash+1;
    int f = 0;
    while(path[i] != '\0'){
        fileName[f] = path[i];
        f++;
        i++;
    }
    if(fileName[i-1] == '/')
        fileName[i-1] = '\0';
    fileName[i] = '\0';

    return fileName;
}


char* setCurrThreadPath(int threadid){
    char newThreadPath[80] = "./cpyfiles/Thread";
    string threadNumString;
    stringstream ss;
    ss << threadid;
    ss >> threadNumString;
    char threadNumChar[2];
    strcpy(threadNumChar, threadNumString.c_str());
    strcat(newThreadPath, threadNumChar);
    char* fileString = "files/";
    strcat(newThreadPath, fileString);
    char *returnString = new char[80];
    strcpy(returnString, newThreadPath);
    return returnString;
}

void *sendFilesFunc(void *threadinput){

    int threadid;
    threadid = *((int*)(&threadinput));
    threadid++;

    //============ Constructs Socket ============
    int sockfd;
    int portnum = 3759;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char buffer[256];

    if((server=gethostbyname(argVec[1].c_str())) == NULL){
        perror("Client- gethostbyname() was not successful!");
        exit(1);
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("Client- socket() was not successful! ");
        exit(1);
    }
    else{}
        cout << "Client- socket() was successful!" << endl;

    serveraddr.sin_family = AF_INET;
    cout << "Server-Using " << argVec[1] << " and port " << portnum << endl;
    serveraddr.sin_port = htons(portnum);
    serveraddr.sin_addr = *((struct in_addr *)server->h_addr);
    memset(&(serveraddr.sin_zero), '\0', 8);


    if(connect(sockfd, (struct sockaddr *)&serveraddr, sizeof(struct sockaddr)) == -1){
        perror("Client- connect() was not successful!");
        exit(1);
    }
    else
        cout << "Client- connect() was successful!" << endl;

    int argCount = argVec.size()-2;
    if( write(sockfd, &argCount, sizeof(int)) < 0 ){
        perror("Client- write() was not successful!");
        exit(1);
    }

    //============ Make cpyfiles/Thread#files directory ============
    char type;

    char newThreadPath[80];
    char *tempPath = setCurrThreadPath(threadid);
    strcpy(newThreadPath, tempPath);
    delete tempPath;

    if(mkdir(newThreadPath,0777))
        perror(newThreadPath);

    //============ Loop through each argument to copy ============
    for( int i = 2 ; i < argVec.size() ; i++ ){
        cout << "Thread " << threadid << " copying " << argVec[i] << " from Server." << endl;
        
        //============ Check to see what if arg is reg/dir ============
        char currArg[80];
        strcpy(currArg, argVec[i].c_str());
        write(sockfd, currArg, 255);
        read(sockfd, &type, sizeof(char));

        switch(type){
            case 'r':{
                //============ Copy Regular File ============
                char newRegPath[80];
                strcpy(newRegPath, newThreadPath);
                strcat(newRegPath, getFileName(currArg));
                int newfd = open(newRegPath,O_RDWR|O_CREAT|O_TRUNC,00666);
                sleep(2);
                int sizeOfFile;
                read(sockfd, &sizeOfFile, sizeof(int));
                if(sizeOfFile > 0)
                    sockToFile(sockfd, newfd);

                close(newfd);
                break;
            }
            case 'd':{
                //============ Copy Directory ============
                char newDirPath[80];
                strcpy(newDirPath, newThreadPath);
                char *tempArg = getFileName(currArg);
                strcat(newDirPath, tempArg);
                delete tempArg;

                if(mkdir(newDirPath,0777))
                     perror(newDirPath);
                sleep(2);

                char newFilebuf[256];
                int sizeOfFile;
                int doneCopying = 1;

                //============ Loop to copy each file ============
                while(1){
                    bzero(newFilebuf,256);
                    read(sockfd, newFilebuf, 255);
                    //============ Sentinel Signal form Server ============
                    if( strcmp(newFilebuf,"stopreading.stop") == 0 )
                        break;
                    char newFilePath[100];
                    char* slash = "/";
                    strcpy(newFilePath, newDirPath);
                    strcat(newFilePath, slash);
                    strcat(newFilePath, newFilebuf);
                    //============ Open/Create new file ============
                    int newfd = open(newFilePath,O_RDWR|O_CREAT|O_TRUNC,00666);
                    read(sockfd, &sizeOfFile, sizeof(int));
                    //============ If size < 0, copy bytes ============
                    if(sizeOfFile > 0)
                        sockToFile(sockfd, newfd);
                    write(sockfd, &doneCopying, sizeof(int));
                    close(newfd);
                }
                break;
            }
            default:{
                //============ Arg might not exist not be reg/dir ============
                cout << "Arguement: " << argVec[i] << " is not a regular file or directory. Cannot by copied." << endl;
                break;
            }
        }
    }
    
    cout << "Thread " << threadid << " finished copying." << endl;
    pthread_exit(NULL);
}
