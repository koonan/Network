#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include <vector>
#include <sys/socket.h>
#include <netdb.h>
#include <iterator>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>

#define MAXDATA 1024
using namespace std;

vector<string> split(string command) {
    stringstream ss(command);
    istream_iterator<string> begin(ss);
    istream_iterator<string> end;
    vector<string> vstrings(begin, end);
    return vstrings;
}

string get_file_type(string file_name){
    if (file_name.compare(file_name.size() - 3, 3, "txt") == 0) return "text/plain";
    if (file_name.compare(file_name.size() - 3, 3, "jpg") == 0) return "image/jpg";
    if (file_name.compare(file_name.size() - 3, 4, "html") == 0) return "text/html";
    return "text/plain";
}

void send_header(int fd, string method, string file_name) {
    string msg = method + " " + file_name + " HTTP/1.0\r\n";
    msg += "Content-Type: " + get_file_type(file_name) + "text/html\r\n";
    msg += "\r\n";
    send(fd, msg.c_str(), MAXDATA, 0);
}

bool recv_ack_from_server(int fd) {
    char httpmsg[MAXDATA];
    recv(fd, httpmsg, MAXDATA, 0);
    cout << "ack " << httpmsg << endl;
    vector<string> splitted_header = split(httpmsg);
    if(splitted_header[1] == "200") {
        return true;
    }
    return false;
}

void send_file_to_server(int sockfd, string file_name){
    struct stat file_stat;
    ssize_t bytes;
    int sent_bytes = 0;
    char file_size[256];
    off_t offset;
    int remain_data;

    int fd = open(file_name.c_str(), O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Error opening file %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (fstat(fd, &file_stat) < 0) {
        fprintf(stderr, "Error fstat --> %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    sprintf(file_size, "%jd", file_stat.st_size);

    // send file size
    bytes = send(sockfd, file_size, MAXDATA, 0);
    if (bytes < 0)
    {
        fprintf(stderr, "Error on sending file size %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    offset = 0;
    remain_data = file_stat.st_size;
    cout << "file size = " << file_size << endl;
    while (((sent_bytes = sendfile(sockfd, fd, &offset, MAXDATA)) > 0) && (remain_data > 0)) {
        remain_data -= sent_bytes;
        cout << "remain data = " << remain_data << endl;
    }
}

void recv_file_from_server(int sockfd, string file_name){
    char httpmsg[MAXDATA];
    FILE *received_file;
    int file_size, remain_data, bytes;

    // receive file size
    recv(sockfd, httpmsg, MAXDATA, 0);
    file_size = atoi(httpmsg);
    received_file = fopen(file_name.c_str(), "w");
    if (received_file == NULL) {
        fprintf(stderr, "Failed to open file %s\n", strerror(errno));

        exit(EXIT_FAILURE);
    }

    remain_data = file_size;
    cout << "File size = " << file_size << endl;

    // receive file data
    while (remain_data > MAXDATA) {
        bytes = recv(sockfd, httpmsg, MAXDATA, 0);
        fwrite(httpmsg, sizeof(char), bytes, received_file);
        remain_data -= bytes;
        fprintf(stdout, "Receive %d bytes and we hope :- %d bytes\n", bytes, remain_data);
    }

    while (remain_data > 0) {
        bytes = recv(sockfd, httpmsg, remain_data, 0);
        fwrite(httpmsg, sizeof(char), bytes, received_file);
        remain_data -= bytes;
        fprintf(stdout, "Receive %d bytes and we hope :- %d bytes\n", bytes, remain_data);

        if(bytes == 0) {
            cout << "There is a problem in receiving the rest of the data" << endl;
            break;
        }
    }

    fclose(received_file);
}

int main(int argc, char *argv[]) {
    string command;
    int sockfd, status;
    struct addrinfo hints;
    struct addrinfo *res;

    // Open the TCPConnection
    if (argc != 3) {
        fprintf(stderr,"Insufficient params\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // getaddrinfo() system call
    if ((status = getaddrinfo(argv[1], argv[2], &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    // socket() system call
    if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
        perror("client: socket");
        exit(1);
    }

    clock_t begin = clock();

    // connect() system call
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("client: connect");
        exit(1);
    }

    // Process the commands
    ifstream commandsfile ("commands.txt", ios::in);
    if(commandsfile.is_open()) {
        while(getline(commandsfile, command)) {
            vector<string> splittedCommand = split(command);
            send_header(sockfd, splittedCommand[0], splittedCommand[1]);
            bool positive_ack = recv_ack_from_server(sockfd);
            if (splittedCommand[0] == "GET") {
                if(positive_ack) {
                    recv_file_from_server(sockfd, splittedCommand[1]);
                } else {
                    cout << "Error 404 file not found" << endl;
                }
            } else if (splittedCommand[0] == "POST") {
                if(positive_ack) {
                    send_file_to_server(sockfd, splittedCommand[1]);
                } else {
                    cout << "Error in POST of file " << splittedCommand[1] << endl;
                }
            }
        }
        commandsfile.close();
    }

    clock_t end = clock();
    double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
    cout << "-------------------------------------------" << endl;
    cout << "\nElapsed time = " << elapsed_secs << " secs\n" << endl;
    cout << "-------------------------------------------" << endl;

    // Close the TCPConnection
    freeaddrinfo(res);
    shutdown(sockfd, 2);
    return 0;
}