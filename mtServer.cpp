/*
* First Name: Ju Yong
* Last Name: Yoon
* Email address: mikejuyoon@gmail.com
*
* Server that sends client files
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
#include <sys/types.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <sys/sendfile.h>
#include <sys/wait.h>

using namespace std;

void sendThisFile(int currfd, int clientfd){
	struct stat buf;
	fstat(currfd, &buf);
	int fsize = buf.st_size;
	int byteswrit = sendfile(clientfd, currfd, NULL, fsize);
	if(byteswrit != fsize){
		cout << "sendfile() did not send all data." << endl;
	}
}

int main(int argc, char* argv[]){

	int sockfd, clientfd = -1;
	int portnum =	 3759;
	struct sockaddr_in serveraddr, cliaddr;
	socklen_t clilen;
	char buffer[256];
	int countBuf = 0;
	
	//====== make socket ======
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1){
		perror("socket() was not successful!");
		exit(1);
	}
	else
		cout << "socket() was sucessful..." << endl;

	memset(&serveraddr, 0 , sizeof(serveraddr));
	memset(&cliaddr, 0 , sizeof(cliaddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(portnum);
	serveraddr.sin_addr.s_addr = INADDR_ANY;

	if( bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0){
		perror("bind() not successful!");
		exit(1);
	}
	else
		cout << "bind() sucessful!" << endl;

	if( listen(sockfd, 10) < 0 ){
		perror("listen() not successful!");
		exit(1);
	}
	else
		cout << "listen() successful!"<< endl;

	int forkcount = 0;

	//enters loops to accept clients.
	while( clientfd < 0 ){
		cout << "accept()ing...." << endl;
		clientfd = accept(sockfd, (struct sockaddr *) &cliaddr, &clilen);

		if(clientfd < 0){
			perror("accept() not successful!");
			exit(1);
		}

		if( clientfd >= 0 ){
			forkcount++;
			int childpid = fork();
			if(childpid < 0){
				cout << "fork() unsuccessful!" << endl;
				exit(1);
			}
			else if(childpid == 0){

				if( read(clientfd, &countBuf, sizeof(int)) < 0 ){
					perror("Error reading (countBuf) from socket!");
					exit(1);
				}

				for(int i = 0 ; i < countBuf ; i++){
					char path[256];
					read(clientfd, path, 255);

					struct stat fileStat;
					lstat(path , &fileStat);

					if(open(path,  O_RDONLY) < 0){
						char type = 'n';
						write(clientfd, &type, sizeof(char));
					} 
					else if(S_ISREG(fileStat.st_mode)){
						char type = 'r';
						write(clientfd, &type, sizeof(char));
						sleep(1);
						int currfd;
						if( (currfd = open(path, O_RDONLY) )< 0 ){
							cout << "Couldn't open file " << path << endl;
						}
						else{
							struct stat nfileStat;
							lstat(path, &nfileStat);
							int sizeOfFile = nfileStat.st_size;
							write(clientfd, &sizeOfFile, sizeof(int));
							sendThisFile(currfd , clientfd);
						}

					}
					else if(S_ISDIR(fileStat.st_mode)){
						char type = 'd';
						write(clientfd, &type, sizeof(char));
						sleep(2);
						int waiting;
						
						DIR *dirp;
						struct dirent* direntp;
						if(! (dirp = opendir(path)) ){
							cout << "Couldn't open " << path << endl;
						}
						while(direntp = readdir(dirp)){
							if(direntp->d_name[0] == '.')
								continue;
							char currfile[80];
							strcpy(currfile,path);
							char* slash = "/";
							strcat(currfile, slash);
							strcat(currfile, direntp->d_name);
							struct stat nfileStat;
							lstat(currfile, &nfileStat);

							if(S_ISREG(nfileStat.st_mode) && ! S_ISLNK(nfileStat.st_mode)){
								char* currFileName = direntp->d_name;
								int currfd;
								if( (currfd = open(currfile, O_RDONLY) )< 0 ){
									continue;
								}
								else{
									write(clientfd, currFileName, 255);
									int sizeOfFile = nfileStat.st_size;
									write(clientfd, &sizeOfFile, sizeof(int));
									sendThisFile(currfd, clientfd);
									read(clientfd, &waiting, sizeof(int));
								}
							}
						}
						char* stopMessage = "stopreading.stop";
						write(clientfd, stopMessage, 255);
						cout << "Finished sending to client." << endl;


					}
				}

				close(sockfd);
				close(clientfd);
				return 0;
			}
		}
		clientfd = -1;
	}

	for( int i = 0 ; i < forkcount ; i++ ){
		wait(NULL);
	}

	return 0;
}
