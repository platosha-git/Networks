#include "sockinc.h"

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
    
    
    if (inet_aton(IP, &addr.sin_addr) == 0) {
        perror("inet_aton failed");
        return EXIT_FAILURE;
    }

        
    char msg[MSG_LEN];
    printf("Input message: ");
    fgets(msg, MSG_LEN, stdin);
        
    ssize_t err = sendto(sock, msg, strlen(msg), 0, (struct sockaddr *) &addr, sizeof(addr));
    if (err == -1) {
        close(sock);
        perror("sendto failed");
        return EXIT_FAILURE;
    }
        
    close(sock);
    printf("Sent msg: %s\n", msg);

    return 0;
}
