#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <chrono>
#include <string>
#include <iostream>
#include <fstream>
#include <thread>
#include <cstdlib>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <signal.h>
#include "common.h"

using namespace std;

struct statusCodes
{
	char* statusCode;
	char* message;
};

struct existingURLS
{
	char* url;
	char* htmlPage;
};


static statusCodes OK = {"200", "OK"};
static statusCodes NotFound = {"404", "Not Found"};
static statusCodes IternalError = {"500", "Iternal error"};

static existingURLS existingURI[3] = {{"/test.html", "test.html"},{"hello.html", "<div><h1>Hello world!</h1></div>"}};

string client_handler(char *message);

// This class manages a thread pool that will process requests
class ThreadPool 
{
public:
	ThreadPool() : done(false) {
		auto numberOfThreads = thread::hardware_concurrency();
		if (numberOfThreads == 0) {
			numberOfThreads = 1;
		}

		for (unsigned i = 0; i < numberOfThreads; ++i) {
			threads.push_back(std::thread(&ThreadPool::doWork, this));
		}
	}

	~ThreadPool() {
		done = true;

		workQueueConditionVariable.notify_all();
		for (auto& thread : threads) {
	  		if (thread.joinable()) {
				thread.join();
	  		}
		}
  	}

  	void queueWork(int fd, char* request) {
		lock_guard<mutex> g(workQueueMutex);
		workQueue.push(pair<int, char*>(fd, request));
		workQueueConditionVariable.notify_one();
  	}

private:
  
  	condition_variable_any workQueueConditionVariable;
  	vector<thread> threads;
  	mutex workQueueMutex;
  	queue<pair<int, char*>> workQueue;
  	
  	bool done;

  	void doWork() {
		while (!done) {
	  		pair<int, char*> request; 
	  		{
				unique_lock<mutex> g(workQueueMutex);
				workQueueConditionVariable.wait(g, [&]{
		  			return !workQueue.empty() || done;
				});

				request = workQueue.front();
				workQueue.pop();
	  		}

	  		processRequest(request);
		}
	}

	void processRequest(const pair<int, char*> item) 
	{
		string s = client_handler(item.second);
		send(item.first, s.c_str(), s.size(), 0);
		close(item.first);
	}
};

const int MAX_CLIENTS = 5;
static int cur_clients = 0;
static int sock = 0;

void signal_handler(int sig)
{
    printf("\nCatched signal CTRL+C\n");
    printf("Server stopping ...\n");

    close(sock);
    exit(0);
}

string get_body(char* fileName)
{
  	string content = "";
  	string line = "";
  	
  	ifstream in(fileName);
  	if (in.is_open()) {
		while (getline(in, line)) {
			content.append(line);
		}
		
		in.close();
  	}
  
  	return content;
}

void log_user(string user_name, char* url)
{
	ofstream of;
  	of.open(user_name + ".txt", ios_base::app);
  	if (of.is_open()) {
		of << user_name + " visited page: " + url << endl;
		of.close();
  	}
}

string form_response(char* headers, int page_id)
{
  	string response = "";
  	statusCodes status_code;
  	if (page_id != -1) {
		status_code = OK;
  	}
  	else {
		status_code = NotFound;
  	}
  
  	response.append(HttpVersion);
  	response.append(" ");

  	response.append(status_code.statusCode);
  	response.append(" ");
  	
  	response.append(status_code.message);
  	response.append("\r\n");
  	
  	response.append("Connection: closed\r\n");
  	response.append("Content-Type: text/html; charset=UTF-8\r\n");
  	response.append("\r\n");
  	
  	if (page_id != -1) {
		string body = get_body(existingURI[page_id].htmlPage);
		if (body != "") {
	  		response.append(body);
		}
  	}
  	
  	return response;
}


string handle_request(char* request)
{
	char* method = strtok(request, " ");
	char* url = strtok(NULL, " ");
	char* http_version = strtok(NULL, "\r\n");
	char* user_name = strtok(NULL, "\r\n");
	char* host_name = strtok(NULL, "\r\n");

	cout << user_name << endl;
	char *name = strstr(user_name, ": ");
  
	if (strcmp(method, "GET") != 0) {
		return "Only GET available!";
	}

	int page_id = -1;
	for (int i = 0; i < 3; i++) {
		if (strcmp(url, existingURI[i].url) == 0) {
			log_user((string)(name + 2), url);
			page_id = i;
			break;
		}
	}
	
	return form_response(request, page_id); 
}


string client_handler(char *request)
{
	printf("Request: %s\n", request);
	string response_str = handle_request(request);
	
	const char* response = response_str.c_str();
	printf("Response: %s\n", response);
	
	return response;
}

int new_client_handler(int *new_sock)
{
	struct sockaddr_in addr;
    int addr_size = sizeof(addr);
    
    *new_sock = accept(sock, (struct sockaddr*) &addr, (socklen_t*) &addr_size);
    if (*new_sock < 0) {
        close(sock);
        perror("accept failed");
        return EXIT_FAILURE;
    }

    cur_clients++;
	printf("\nClient %d (fd = %d) connected!\n\n", cur_clients, *new_sock);
    return 0;
}

int main()
{
	struct sockaddr_in client_addr;
  
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

	if(bind(sock, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
		perror("bind failed");
		return EXIT_FAILURE;
	}

	signal(SIGINT, signal_handler);

	if (listen(sock, MAX_CLIENTS) < 0) {
		close(sock);
		perror("listen failed");
		return EXIT_FAILURE;
	}

	printf("Server running ...\n\n");
	
	ThreadPool tp;
	
	while(1) {
		int new_sock = 0;
		int exit_code = new_client_handler(&new_sock);
		if (exit_code != 0) {
			return EXIT_FAILURE;
		}

      	char buf[MESSAGE_LEN];
      	int size = recv(new_sock, buf, MESSAGE_LEN, 0);
      	buf[size] = '\0';

      	tp.queueWork(new_sock, buf);
	}

	close(sock);
	return 0;
}
