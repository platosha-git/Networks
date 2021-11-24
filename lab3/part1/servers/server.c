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

#define FILES_DIR "./files"
#define BUF_LEN 512
#define MAX_COUNT 8

int fd_socket;

void sigint_catcher(int signum)
{
    printf("Ctrl+C caught - closing socket \n");
    close(fd_socket);
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

int main()
{
    printf("Server starting ...\n");
    chdir(FILES_DIR);

    fd_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_socket < 0) {
        printf("Failed create socket: %s\n", strerror(errno));
        return -errno;
    }
 
    signal(SIGINT, sigint_catcher);
    struct sockaddr_in server_addr, client_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
 
    if (bind(fd_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Failed bind socket: %s\n", strerror(errno));
        return errno;
    }
 
    if (listen(fd_socket, MAX_COUNT) < 0) {
        printf("Failed listen socket: %s\n", strerror(errno));
        close(fd_socket);
        return errno;
    }

    printf("Server running ...\n");
    int client_sockets[MAX_COUNT] = {0};
    int client_socket = 0;
    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd_socket, &rfds);
        int max_fd = fd_socket;
        for (int i = 0; i < MAX_COUNT; i++) {
            int fd = client_sockets[i];
            if (fd > 0)
                FD_SET(fd, &rfds);
            max_fd = (fd > max_fd) ? (fd) : (max_fd);
        }
        int updates_cl_count = pselect(max_fd + 1, &rfds, NULL, NULL, NULL, NULL);
        if (FD_ISSET(fd_socket, &rfds)) {
            struct sockaddr_in client_addr;
            int client_addr_size = sizeof(client_addr);
            int accepted_socket = accept(fd_socket, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_size);
            if (accepted_socket < 0) {
                printf("Failed accept: %s", strerror(errno));
                close(fd_socket);
                return errno;
            }
            
            int added_flag = 0;
            for (int i = 0; i < MAX_COUNT && !added_flag; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = accepted_socket;
                    added_flag = 1;
                }
            }

            printf("New client: fd = %d, address = %s:%d\n", accepted_socket, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            //send_filenames(accepted_socket);
        }
        char filename[BUF_LEN];
        for (int i = 0; i < MAX_COUNT; i++) {
            int fd = client_sockets[i];
            if ((fd > 0) && FD_ISSET(fd, &rfds)) {
                if (recv_filename(fd, filename, BUF_LEN) < 0 || send_file(fd, filename) < 0) {
                    close(fd);
                    client_sockets[i] = 0;
                }
            }
        }
    }
    return 0;
}