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
    printf("Server will stop\n");

    close(sock);
    exit(0);
}

size_t read_files(char* buffer, size_t buf_size)
{
    DIR *d = opendir(".");
    if (!d) {
        printf("Error while reading files\n");
        return -1;
    }

    buffer[0] = 0;
    size_t cur_size = 0;
    struct dirent *dir;
    size_t count_files = 0;
    while((dir = readdir(d)) != NULL) {
        if (strcmp(dir->d_name, ".") && strcmp(dir->d_name, "..")) {
            size_t filename_size = strlen(dir->d_name) + 1;
            if (filename_size + cur_size < buf_size) {
                strcat(buffer, dir->d_name);
                cur_size += filename_size;
                buffer[cur_size - 1] = '\n';
            }
            count_files++;
        }
    }
    closedir(d);
    
    return count_files;
}

int send_filenames(int fd)
{
    char buffer[BUF_LEN];
    size_t count_files = read_files(buffer, BUF_LEN);
    if (send(fd, buffer, strlen(buffer) + 1, 0) < 0) {
        printf("Failed send filenames: %s", strerror(errno));
        return errno;
    }
 
    return 0;
}

int recv_filename(int fd, char* filename, size_t buffer_size)
{
    struct sockaddr_in client_addr;
    int client_addr_size = sizeof(client_addr);
    getpeername(fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);
    int size = recv(fd, filename, buffer_size, 0);
    if (size <= 0) {
        cur_clients--;
        printf("Client %s:%d disconnected!\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        return -1;
    }

    filename[size] = 0;
    if (filename[size - 1] == '\n')
        filename[size - 1] = 0;
    printf("Client %s:%d requested file: %s.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), filename);
    return 0;
}

int send_file(int fd, const char* filename)
{
    int code_error = 0;
    int file_fd = open(filename, O_RDONLY);
    int64_t file_size = -1;
    struct stat file_stat;
    if (file_fd > 0 && fstat(file_fd, &file_stat) == 0 && S_ISREG(file_stat.st_mode))
        file_size = file_stat.st_size;
    printf("The size of the transferred file: %ld\n", file_size);
    int64_t tmp_file_size = htonll(file_size);
    
    if (send(fd, &tmp_file_size, sizeof(int64_t), 0) < 0) {
        printf("Failed send size of file: %s", strerror(errno));
        code_error = errno;
    }
    
    if (!code_error && file_size >= 0) {
        char packet[PACKET_SIZE];
        ssize_t read_bytes;
        
        while (!code_error && (read_bytes = read(file_fd, packet, PACKET_SIZE - 1)) > 0) {
            packet[read_bytes] = 0;
            if (send(fd, packet, read_bytes, 0) < 0) {
                printf("Failed send file packet: %s", strerror(errno));
                code_error = errno;
            }
            // printf("send chunck: %s.\n", packet);
        }
    }
 
    if (file_fd > 0)
        close(file_fd);
    return code_error;
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
    printf("\nClient %d (fd = %d)\n\n", cur_clients, new_sock);
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
 
    ssize_t exit_code = bind(sock, (struct sockaddr*) &addr, sizeof(addr));
    if (exit_code < 0) {
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

        int active_client = pselect(max_fd + 1, &set, NULL, NULL, NULL, NULL);
        if (active_client < 0) {
            perror("pselect failed");
            return EXIT_FAILURE;
        }

        if (FD_ISSET(sock, &set)) {
            exit_code = new_client_handler(clients);
            if (exit_code != 0) {
                return exit_code;
            }
        }

        process_clients(&set, clients);
    }

    return 0;
}
