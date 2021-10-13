#include "sockinc.h"

int convert_num(int dec_num, const int base)
{
    int res = 0, i = 1;
    while (dec_num > 0) {
        
        res += (dec_num % base) * i;
        dec_num /= base;
        i *= 10;
    }

    return res;
}

int main(void)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) {
        perror("socket failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr;
    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);    


    if (bind(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind failed");
        return EXIT_FAILURE;
    }
        

    printf("Listen port %d.\n", PORT);

    char msg[MSG_LEN];
    while (1) {
        int len = sizeof(addr);
        if (recvfrom(sock, msg, sizeof(msg), 0, (struct sockaddr *) &addr, &len) == -1) {
            perror("recvfrom");
            return EXIT_FAILURE;
        }
        
        printf("Received message %s\n", msg);

        int dec_num = atoi(msg);
        int bin_num = convert_num(dec_num, 2);
        int hex_num = convert_num(dec_num, 16);
        int oct_num = convert_num(dec_num, 8);
        int my_num = convert_num(dec_num, 9);

        printf("dec_num = %d\n", dec_num);
        printf("bin_num = %d\n", bin_num);
        printf("hex_num = %d\n", hex_num);
        printf("oct_num = %d\n", oct_num);
        printf("my_num = %d\n", my_num);
    }

    close(sock);

    return 0;
}
