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

int get_file(int socket, const char* filename)
{
    int64_t size = 0;
    
    ssize_t exit_code = recv(socket, &size, sizeof(size), 0);
    if (exit_code == -1) {
        perror("recv size failed");
        return EXIT_FAILURE;
    }
 
    size = ntohll(size);
    if (size < 0) {
        printf("File not found!\n");
        return 0;
    }
    
    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
    
    exit_code = 0;
    char packet[PACKET_SIZE];
    size_t i = 0;
    while (i < size) {
        size_t size = recv(socket, packet, PACKET_SIZE, 0);
        if (size == -1) {
            perror("recv content failed");
            exit_code = EXIT_FAILURE;
            break;
        }

        packet[size] = 0;
        write(fd, packet, size);
        i += size;
    }
 
    close(fd);
    return exit_code;
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
    
    ssize_t exit_code = connect(sock, (struct sockaddr*) &addr, sizeof(addr));
    if (exit_code == -1) {
        perror("connect failed");
        return EXIT_FAILURE;
    }

    char filename[NAME_LEN];
    printf("Input filename: ");
    fgets(filename, NAME_LEN, stdin);
    printf("\n");

    size_t size = strlen(filename);
    if (filename[size - 1] == '\n') {
        filename[size - 1] = 0;
        size--;
    }
    
    exit_code = sendto(sock, filename, size, 0, (struct sockaddr *) &addr, sizeof(addr));
    if (exit_code == -1) {
        close(sock);
        perror("sendto failed");
        return EXIT_FAILURE;
    }
        
    exit_code = get_file(sock, filename);    
    close(sock);

    return exit_code;
}
