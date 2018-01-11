#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <fstream>
#include <strings.h>
#include <stdlib.h>
#include <string>
#include <pthread.h>
#include <sstream>
#include <ctime>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

using namespace std;
#define BACKLOG 10
#define PORTNO 80
#define MAXDATA 1024
#include <fcntl.h>

//For TimeOut to close the connection

struct timeval timeOut[BACKLOG+1];
int sockets[BACKLOG + 1];
int t ;
int thread_no[BACKLOG + 1];

//Declared Function
string t_string(int a);
string get_file_type(string file_name);
void* header (string file_name, int handler, int status );
void recv_file_from_client(int sockfd, string file_name);
void send_file_to_client(int handler, char*  filename);
void *handle_request(void *param);

//Start Program From Here
int main(int argc, char* argv[])
{
    cout << "Start The Server" << endl;
    int noOfThreads = 0;
    int portNo, listenFd;
    socklen_t clnAddLen;
    struct sockaddr_storage clntAdd;
    struct addrinfo hints, *svrAdd;
    pthread_t threadA[BACKLOG];

    if (argc < 2)
    {
        cout << "missed port number, we use default port number 8080" << endl;
        portNo = PORTNO;
    }
    else
    {
        portNo = atoi(argv[1]);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    int status ;
    if ((status = getaddrinfo(NULL, t_string(portNo).c_str(), &hints, &svrAdd)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }
    status = 1;
    listenFd = socket(svrAdd->ai_family, svrAdd->ai_socktype,svrAdd->ai_protocol);
    if(listenFd < 0)
    {
        cerr << "Cannot open new socket" << endl;
        return 1;
    }
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &status, sizeof(status));
    if(bind(listenFd, svrAdd->ai_addr, svrAdd->ai_addrlen) < 0)
    {
        cerr << "Cannot biond" << endl;
        return 1;
    }
    freeaddrinfo(svrAdd);
    listen(listenFd, BACKLOG);
    while(true)
    {
        thread_no[noOfThreads ] =  noOfThreads ;
        clnAddLen = sizeof(clntAdd);
        int connFd;
        sockets[noOfThreads] = accept (listenFd, (struct sockaddr *) &clntAdd, &clnAddLen);
        if (noOfThreads == 0)
        {
            timeOut[noOfThreads].tv_sec= 20 ;
            t = 20;
            cout << "here" <<endl;
        }

        cout << "*******************request from: " << sockets[noOfThreads] << endl ;
        cout << "noOfThreads" << noOfThreads <<endl;
        if (  sockets[noOfThreads]  < 0 )
        {
            cout << "cannot accept the connection" << endl;
            return 1 ;
        }
        else
        {
            cout << "connection accepted "<< endl;
        }
        //Create for each client thread
        if (noOfThreads < BACKLOG)
        {
            int status_t = pthread_create(&threadA[noOfThreads], NULL, handle_request,&thread_no[noOfThreads ]);
            cout << " threadsssss" << noOfThreads << endl;
            if (status_t != 0)
            {
                fprintf(stderr, "Error creating thread\n");
                return -1;
            }
            timeOut[noOfThreads].tv_sec= t;
            noOfThreads++;
            thread_no[noOfThreads] = noOfThreads;

            for(int i = 0 ; i < noOfThreads ; i++)
            {
                if ( timeOut[i].tv_sec != 0 )
                    if( timeOut[i].tv_sec > t && sockets[i] != -1)
                    {
                       cout << "time before " << timeOut[i].tv_sec << endl;
                        timeOut[i].tv_sec= t;
                        cout << "Time started " <<  timeOut[i].tv_sec<<endl;
                    }
            }

             cout << "TTTTTTTtt" << t <<endl;
            t = t - 2 ;
        }
        else
        {
            noOfThreads = 0;
            thread_no[noOfThreads ] =  noOfThreads ;
            for (int j=9; j>=0; j--)
            {
                if (pthread_join(threadA[j], NULL))
                {
                    fprintf(stderr, "Error joining thread (timeout) \n");
                    return -2;
                }
            }

            sockets[noOfThreads] = sockets[BACKLOG];

            int status_t = pthread_create(&threadA[noOfThreads], NULL,
                                          handle_request, &thread_no[noOfThreads ]);
            if (status_t != 0)
            {
                fprintf(stderr, "Error creating thread\n");
                return -1;
            }
        }
    }
}


//Convert From Integer to String
string t_string(int a)
{

    stringstream ss;
    ss << a;
    return ss.str();
}

//Handle request form client if get or post (param = file Discriptor)
void *handle_request(void *param)
{

    int * thread = (int*)  param;
    int handler = sockets[*thread];
    char buf[MAXDATA];
    char *method;
    char *filename;
    struct timeval tv ;

    while (true  )
    {
        tv.tv_sec = timeOut[*thread].tv_sec ;
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(handler, &readfds);

        // don't care about writefds and exceptfds:
        select(handler+1, &readfds, NULL, NULL, &timeOut[*thread]);

        if (FD_ISSET(handler, &readfds))
        {
            if (  recv(handler, buf, MAXDATA, 0) > 0 )
            {
                method = strtok(buf, " ");
                cout << "Message " << buf  << " : Connection " << handler << endl;

                if (strcmp (method, "GET") != 0  && strcmp (method, "POST") != 0  )
                {
                    cout << "Un recognised message " << endl ;
                    return 0;
                }

                if (strcmp (method, "GET") == 0)
                {
                    filename = strtok(NULL, " ");
                    send_file_to_client(handler, filename);

                }
                else if (strcmp (method, "POST") == 0)
                {
                    filename = strtok(NULL, " ");
                    FILE * received_file = fopen(filename, "w");
                    if (received_file == NULL)
                    {
                        header(filename,handler,1);
                        continue;
                    }
                    header(filename,handler,0);
                    recv_file_from_client(handler, filename);
                }

                memset(&buf,0, sizeof buf);
            }
            else
            {
                cout << "Close Connection" <<endl;
                sockets[*thread] = -1;
                pthread_exit(NULL);
                close(handler);

            }
        }
        else
        {
            printf("Timed out .\n");
            cout << "Connection" <<handler <<endl;
            close(handler);
            sockets[*thread] = -1;
            pthread_exit(NULL);
        }
    }
}
//Receive file from client
void recv_file_from_client(int sockfd, string file_name)
{
    char httpmsg[MAXDATA];
    FILE *received_file;
    int file_size, remain_data, bytes;

    // receive file size
    recv(sockfd, httpmsg, MAXDATA, 0);
    file_size = atoi(httpmsg);
    if (file_size > 0)
    {
        received_file = fopen(file_name.c_str(), "w");
        if (received_file == NULL)
        {
        	cout << "Failed to open file" <<endl;
            exit(EXIT_FAILURE);
        }
        remain_data = file_size;
        // receive file data
        while (remain_data > MAXDATA)
        {
            bytes = recv(sockfd, httpmsg, MAXDATA, 0);
            fwrite(httpmsg, sizeof(char), bytes, received_file);
            remain_data -= bytes;
            cout << "Receive " <<  bytes << "bytes and we hope" << endl;
        }

        while(remain_data > 0)
        {
            bytes = recv(sockfd, httpmsg, remain_data, 0);
            fwrite(httpmsg, sizeof(char), bytes, received_file);
            remain_data -= bytes;
            cout << "Receive " <<  bytes << "bytes and we hope" << endl;
            if (bytes == 0)
            {
                break;
            }
        }
        cout << "Sent" << endl;
        fclose(received_file);
    }
    else
    {
        cout << "Invalid file size!" << endl;
    }
}

void send_file_to_client(int handler, char*  filename)
{
    struct stat file_stat;
    int fd ;
    ssize_t len;
    off_t  offset;
    int sent_bytes = 0;
    int remain_data;

    char buf[MAXDATA];

    if (filename[0] == '/') filename++;
    fd = open(filename, O_RDONLY);

    if (fd == -1)
    {
        cout << "file not found " << endl;
        header(filename,handler,1);
        return ;
    }
    if (fstat(fd, &file_stat) < 0)
    {
        header(filename,handler,1);
        return ;
    }
    header(filename,handler,0);
    sprintf(buf, "%d", file_stat.st_size);
    len = send(handler, buf, MAXDATA, 0);

    if (len < 0)
    {
        cout << "Error on sending greetings --> " <<endl;
    }
    cout <<"Server sent  " << len << "bytes for the size "<< endl ;

    offset = 0;
    remain_data = file_stat.st_size;
    while (((sent_bytes = sendfile(handler, fd, &offset, MAXDATA)) > 0) && (remain_data > 0))
    {
        remain_data -= sent_bytes;
        if (remain_data == 0)
            break ;
    }
}

string get_file_type(string file_name)
{
    if (file_name.compare(file_name.size() - 3, 3, "txt") == 0) return "text/plain";
    if (file_name.compare(file_name.size() - 3, 3, "jpg") == 0) return "image/jpg";
    if (file_name.compare(file_name.size() - 3, 4, "html") == 0) return "text/html";
    return "text/plain";
}

void* header (string file_name, int handler, int status )
{
    char header [MAXDATA] = {0};
    string msg;
    if (status == 0 )
    {
        msg = "HTTP/1.0 200 OK\r\nContent-Type: " + get_file_type(file_name) + "\r\n\r\n";
        sprintf(header, msg.c_str());

    }
    else
    {
        msg = "HTTP/1.0 404 Not Found\r\nContent-Type: " + get_file_type(file_name) + "\r\n\r\n";
        sprintf(header, msg.c_str());
    }
    send (handler, header, MAXDATA, 0 );
}
