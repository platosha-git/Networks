#include "sockinc.h"

#define NUM_BITS 7

static int sock = 0;

void signal_handler(int sig) 
{
    close(sock);
    exit(0);
}

void output_num(const char *msg, const char buffer[NUM_BITS])
{
    printf("%s = ", msg);
    for (int i = 0; i < 7; i++) {
        printf("%c", buffer[i]);
    }
    printf("\n");
}

void convert_num(int dec_num, const int base, char (*buffer)[NUM_BITS], const char *msg)
{
    memset(*buffer, 0, 7);
    int i = 6;

    while (dec_num > 0) {
        int x = dec_num % base;
        dec_num /= base;

        char z;
        if(x < 10) {
            z = '0' + x;
        }
        else {
            z = 'A' - 10 + x;
        }
        
        (*buffer)[i] = z;
        i--;
    }

    output_num(msg, *buffer);
}

int main(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        perror("socket failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr = {
        addr.sin_family = AF_INET,
        addr.sin_port = htons(PORT),
        addr.sin_addr.s_addr = htonl(INADDR_ANY)
    };

    ssize_t err = bind(sock, (struct sockaddr*) &addr, sizeof(addr));
    if (err < 0) {
        close(sock);
        perror("bind failed");
        return EXIT_FAILURE;
    }
        
    signal(SIGINT, signal_handler);
    printf("Listen port %d.\n\n", PORT);

    char msg[MSG_LEN];
    while (1) {
        int len = sizeof(addr);
        err = recvfrom(sock, msg, sizeof(msg), 0, (struct sockaddr *) &addr, &len);
        if (err == -1) {
            perror("recvfrom");
            return EXIT_FAILURE;
        }

        char buffer[NUM_BITS];
        int dec_num = atoi(msg);

        printf("dec_num = %d\n", dec_num);
        convert_num(dec_num, 2, &buffer, "bin_num");
        convert_num(dec_num, 16, &buffer, "hex_num");
        convert_num(dec_num, 8, &buffer, "oct_num");
        convert_num(dec_num, 9, &buffer, "my_num");
    }

    return 0;
}
