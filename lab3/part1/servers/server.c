#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include "../common.h"

#define BUF_LEN 512
#define MAX_CLIENTS 5

static int cur_clients = 0;
static int sock = 0;

void signal_handler(int sig)
{
    printf("\nCatched signal CTRL+C\n");
    printf("Server stopping ...\n");

    close(sock);
    exit(0);
}

int get_files()
{
    char buffer[BUF_LEN] = { 0 };

    DIR *d = opendir(".");
    if (!d) {
        printf("Error while reading files\n");
        return -1;
    }

    buffer[0] = 0;
    size_t cur_size = 0;
    struct dirent *dir;
    while((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")) {
            size_t filename_size = strlen(dir->d_name) + 1;
            if (filename_size + cur_size < BUF_LEN) {
                strcat(buffer, dir->d_name);
                cur_size += filename_size;
                buffer[cur_size - 1] = '\n';
            }
        }
    }
    closedir(d);

    printf("\nServer directory files:\n%s\n", buffer);
    return 0;
}

int recv_filename(int fd, char* filename, size_t buffer_size)
{
    struct sockaddr_in addr;
    int addr_size = sizeof(addr);
    getpeername(fd, (struct sockaddr*) &addr, (socklen_t*) &addr_size);
    
    int size = recv(fd, filename, buffer_size, 0);
    if (size <= 0) {
        printf("\nClient (fd = %d) disconnected!\n", fd);
        cur_clients--;
        return -1;
    }

    filename[size] = 0;
    if (filename[size - 1] == '\n')
        filename[size - 1] = 0;

    printf("Client (fd = %d) requested file: %s.\n", fd, filename);
    return 0;
}

int send_file(int fd, const char* filename)
{
    int exit_code = 0;

    int file_fd = open(filename, O_RDONLY);
    int64_t file_size = -1;
    struct stat file_stat;

    if (file_fd > 0 && fstat(file_fd, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
        file_size = file_stat.st_size;
    }

    printf("The size of the requested file: %ld\n", file_size);
    int64_t tmp_file_size = htonll(file_size);
    
    if (send(fd, &tmp_file_size, sizeof(int64_t), 0) < 0) {
        perror("failed send");
        exit_code = EXIT_FAILURE;
    }
    
    if (!exit_code && file_size >= 0) {
        char packet[PACKET_SIZE];
        ssize_t read_bytes;
        
        while (!exit_code && (read_bytes = read(file_fd, packet, PACKET_SIZE - 1)) > 0) {
            packet[read_bytes] = 0;
            if (send(fd, packet, read_bytes, 0) < 0) {
                perror("send failed");
                exit_code = EXIT_FAILURE;
            }
        }
    }
 
    if (file_fd > 0) {
        close(file_fd);
    }

    return exit_code;
}

int new_client_handler(int* const clients)
{
    struct sockaddr_in addr;
    int addr_size = sizeof(addr);
    
    int new_sock = accept(sock, (struct sockaddr*) &addr, (socklen_t*) &addr_size);
    if (new_sock < 0) {
        close(sock);
        perror("accept failed");
        return EXIT_FAILURE;
    }
            
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == 0) {
            clients[i] = new_sock;
            break;
        }
    }

    cur_clients++;
    printf("\nClient %d (fd = %d) connected!\n\n", cur_clients, new_sock);
    return 0;
}

void process_clients(fd_set *set, int* const clients)
{
    char filename[BUF_LEN] = { 0 };
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        int fd = clients[i];
        if ((fd > 0) && FD_ISSET(fd, set)) {
            
            if (recv_filename(fd, filename, BUF_LEN) < 0 || send_file(fd, filename) < 0) {
                close(fd);
                clients[i] = 0;
            }
            else {
                printf("File sent successfully!\n");
            }
        }
    }
}

int main()
{
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socker failed");
        return EXIT_FAILURE;
    }
 
    struct sockaddr_in addr = {
        addr.sin_family = AF_INET,
        addr.sin_port = htons(PORT),
        addr.sin_addr.s_addr = htonl(INADDR_ANY)
    };
 
    if (bind(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return EXIT_FAILURE;
    }

    signal(SIGINT, signal_handler);

    if (listen(sock, MAX_CLIENTS) < 0) {
        close(sock);
        perror("listen failed");
        return EXIT_FAILURE;
    }
    
    printf("Server running ...\n");
    if (get_files() < 0 ) {
        return -1;
    }

    int clients[MAX_CLIENTS] = { 0 };
    while (1) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(sock, &set);
        int max_fd = sock;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int cur_client = clients[i];
            if (cur_client > 0) {
                FD_SET(cur_client, &set);
            }

            max_fd = (cur_client > max_fd) ? (cur_client) : (max_fd);
        }

        if (pselect(max_fd + 1, &set, NULL, NULL, NULL, NULL) < 0) {
            perror("pselect failed");
            return EXIT_FAILURE;
        }

        if (FD_ISSET(sock, &set)) {
            int exit_code = new_client_handler(clients);
            if (exit_code != 0) {
                return exit_code;
            }
        }

        process_clients(&set, clients);
    }

    return 0;
}
