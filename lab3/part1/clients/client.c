#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <inttypes.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include "../common.h"

#define MSG_MAX_LEN 512

int recv_filenames(int fd)
{
    char filenames[MSG_MAX_LEN];
    int size = recv(fd, filenames, MSG_MAX_LEN, 0);
    if (!size) {
        printf("Failed get filenames!\n");
        return -1;
    }
    else {
        printf("Server directory files:\n%s\n", filenames);
        return 0;
    }
}

int recv_file(int fd, const char* filename)
{
    int64_t file_size = 0;
    if (recv(fd, &file_size, sizeof(file_size), 0) < 0) {
        printf("Failed get file size!\n");
        return -1;
    }
 
    file_size = ntohll(file_size);
    if (file_size < 0) {
        printf("File not found!\n");
        return 0;
    }
 
    printf("The size of the transferred file: %" PRId64 " byte\n", file_size);
    
    int file_fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
    int code_error = 0;
    char packet[PACKET_SIZE];
    
    for (size_t i = 0; !code_error && i < file_size;) {
        size_t size = recv(fd, packet, PACKET_SIZE, 0);
        if (size < 0) {
            printf("Failed get filenames!\n");
            code_error = -1;
        }
        else {
            packet[size] = 0;
            // printf("recv %s %ld\n", packet, size);
            write(file_fd, packet, size);
            i += size;
        }
    }
 
    close(file_fd);
    return code_error;
}


int main(int argc, char **argv)
{
    int fd_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_socket < 0) {
        printf("Failed create socket: %s\n", strerror(errno));
        return -errno;
    }
 
    struct hostent *host = gethostbyname(SERVER_HOST);
    if (!host) {
        printf("Error gethostbyname: %s\n", strerror(errno));
        return errno;
    }
 
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr = *((struct in_addr *)host->h_addr_list[0]);
    
    if (connect(fd_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed connect to server: %s\n", strerror(errno));
        return errno;
    }
 
    char filename[MSG_MAX_LEN];
    recv_filenames(fd_socket);
    int code_error = 0;
    while (!code_error) {
        printf("Input filename: ");
        fgets(filename, MSG_MAX_LEN, stdin);
        printf("\n");
        size_t size = strlen(filename);
        if (filename[size - 1] == '\n') {
            filename[size - 1] = 0;
            size--;
        }
        
        if (send(fd_socket, filename, size, 0) < 0) {
            printf("Failed send filename to server: %s", strerror(errno));
            return errno;
        }
        
        code_error = recv_file(fd_socket, filename);
    }
    
    close(fd_socket);
    return code_error;
}
