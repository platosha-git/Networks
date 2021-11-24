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

int get_file(int fd, const char* filename)
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
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket failed");
        return EXIT_FAILURE;
    }
 
    struct hostent *server = gethostbyname(HOST);
    if (server == NULL) {
        perror("gethostbyname failed");
        return EXIT_FAILURE;
    }
 
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr = *((struct in_addr *) server->h_addr_list[0])
    };
    
    if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("connect failed");
        return EXIT_FAILURE;
    }
 
    //int exit_code = get_filenames(sock);

    char filename[NAME_LEN];
    printf("Input filename: ");
    fgets(filename, NAME_LEN, stdin);
    printf("\n");

    size_t size = strlen(filename);
    if (filename[size - 1] == '\n') {
        filename[size - 1] = 0;
        size--;
    }
    
    ssize_t err = sendto(sock, filename, size, 0, (struct sockaddr *) &addr, sizeof(addr));
    if (err == -1) {
        close(sock);
        perror("sendto failed");
        return EXIT_FAILURE;
    }
        
    get_file(sock, filename);    
    close(sock);

    return 0;
}
