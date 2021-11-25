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

string get_request(char* user_name, char* url)
{
	string res_str = "";
  	string str = url;
  	
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
  	
  	cout << "Get request: " << res_str << endl;
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

	char user_name[NAME_LEN];
	printf("Input user name: ");
    fgets(user_name, NAME_LEN, stdin);
    size_t size_name = strlen(user_name);
    if (user_name[size_name - 1] == '\n') {
        user_name[size_name - 1] = 0;
        size_name--;
    }
	
	char url[URL_LEN];
	printf("Input URL: ");
	fgets(url, URL_LEN, stdin);
	size_t size_url = strlen(url);
    if (url[size_url - 1] == '\n') {
        url[size_url - 1] = 0;
        size_url--;
    }
	
	string msgstr = get_request(user_name, url);
	const char* msg = msgstr.c_str();

	printf("Sending request ...\n\n");
	
	exit_code = send(sock, msg, MESSAGE_LEN, 0);
	if (exit_code == -1) {
		perror("send failed");
		return EXIT_FAILURE;
	}
	
	char response[MESSAGE_LEN];
	exit_code = recv(sock, response, MESSAGE_LEN, 0);
	if (exit_code == -1) {
		perror("recv failed");
		return EXIT_FAILURE;
	}
	
	printf("The server response: %s\n\n", response);

	close(sock);
	return 0;
}