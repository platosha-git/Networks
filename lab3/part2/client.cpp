#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include "common.h"

using namespace std;

void perror_and_exit(char *s, int exit_code)
{
	perror(s);
	exit(exit_code);
}


string generateGetMessage(char *user_name, char *s)
{
	string res_str = "";
  	string str = s;
  	res_str.append("GET ");
	res_str.append(str + " ");
  	res_str.append("HTTP/1.1\r\n");
  	res_str.append("Username: ");
  	res_str.append(user_name);
  	res_str.append("\r\n");
  	res_str.append("Host: " + str + "\r\n");
  	res_str.append("User-Agent: Console\n");
  	res_str.append("Accept: text/html\n");
  	res_str.append("\0");
  	cout << "request_message: " << res_str << endl;
  	return res_str;
}


int main()
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket failed");
        return EXIT_FAILURE;
	}

	struct sockaddr_in addr = {
		addr.sin_family = AF_INET,
		addr.sin_port = htons(PORT), 
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK)
	};

	ssize_t exit_code = connect(sock, (struct sockaddr*) &addr, sizeof(addr));
	if (exit_code == -1) {
		perror("connect failed");
		return EXIT_FAILURE;
	}

	
	char *buf = (char*)malloc(MESSAGE_LEN);

	char user_name[NAME_LEN];
	printf("Input user name: ");
    fgets(user_name, NAME_LEN, stdin);
    printf("\n");
	
	char message[MESSAGE_LEN];
	printf("enter url: \n");
	scanf("%s", message);
	//printf("you enetered this url: %s\n", message);
	
	//message = "gell";
	string mes = generateGetMessage(user_name, message);

	const char * msg = mes.c_str();

	//printf("The client has this message:\n");
	  //printf("%s\n\n", msg);
	printf("Sending message...\n\n");
	
	if(send(sock, msg, MESSAGE_LEN, 0) < 0)
	{
	  perror_and_exit("send()", 3);
	}
	
	if(recv(sock, buf, MESSAGE_LEN, 0) < 0)
		{
	  perror_and_exit("recv()", 4);
	}
	
	printf("The server returned to client this message:\n");
	printf("%s\n\n", buf);
	free(buf);
	close(sock);

	return 0;
}