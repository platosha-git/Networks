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


static statusCodes OKCode = {"200", "OK"};
static statusCodes NotFoundCode = {"404", "Not Found"};
static statusCodes IternalErrorCode = {"500", "Iternal error"};

static existingURLS existingURI[3] = {{"/test.html", "test.html"},{"/mm.html", "mm.html"},{"klimat05.html", "<div><h1> hello world</h1></div>"}};


string clientHandler(char *message);

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

const int MAX_CLIENTS = 5;

  void processRequest(const std::pair<int, char*> item) {

	string s = clientHandler(item.second);
	
	send(item.first, s.c_str(), s.size(), 0);

	// Close the connection
	close(item.first);
  }
};


string getInfoFromFile(char* fileName)
{
  string fileRes = "";
  string line;
  std:ifstream in(fileName);
  if (in.is_open())
  {
	while (getline(in, line))
	{
		fileRes.append(line);
	}

	in.close();
  }
  return fileRes;
}

void saveUserStatistic(string userId, char* url)
{
  std:ofstream of;
  of.open(userId + ".txt", ios_base::app);
  if (of.is_open())
  {
	of << userId + " visited page: " + url << endl;

	of.close();
  }
}

void perror_and_exit(char *s, int exit_code)
{
	perror(s);
	exit(exit_code);
}

string generateResponseMessage(char* headers, int pageId)
{
  string resultMsg = "";
  statusCodes statusCode;
  if (pageId != -1)
  {
	statusCode = OKCode;
  }
  else
  {
	statusCode = NotFoundCode;
  }
  resultMsg.append(HttpVersion);
  resultMsg.append(" ");
  resultMsg.append(statusCode.statusCode);
  resultMsg.append(" ");
  resultMsg.append(statusCode.message);
  resultMsg.append("\r\n");
  resultMsg.append("Connection: closed\r\n");
  resultMsg.append("Content-Type: text/html; charset=UTF-8\r\n");
  resultMsg.append("\r\n");
  if (pageId != -1)
  {
	string fileRes = getInfoFromFile(existingURI[pageId].htmlPage);
	if (fileRes != "")
	  resultMsg.append(fileRes);
  }
  return resultMsg;
}


string handleRequestMessage(char* innerMessage)
{
	char* method = strtok(innerMessage, " ");
	char* url = strtok(NULL, " ");
	char* httpVersion = strtok(NULL, "\r\n");
	char* userName = strtok(NULL, "\r\n");
	char* HostName = strtok(NULL, "\r\n");
	cout << "+++" << userName << "+++" << endl;
	char *Name = strstr(userName, ": ");
  
	if (strcmp(method, "GET") != 0) {
		return "It's not Get";
	}

	int pageId = -1;
	for (int i = 0; i < 3; i++) {
		if (strcmp(url, existingURI[i].url) == 0) {
			saveUserStatistic((string)(Name + 2), url);
			pageId = i;
			break;
		}
	}
	
	char res[MESSAGE_LEN];
	return generateResponseMessage(innerMessage, pageId); 
}


string clientHandler(char *message)
{
	printf("Client's message: %s\n", message);
	string res = handleRequestMessage(message);
	//cout <<"test;" << res << endl;
	
	const char * msg = res.c_str();
	printf("====================================\n");
	printf("Server's message:\n %s", msg);
	
	return msg;
	
	//send(sock, msg, strlen(msg), 0);
	//cout << "ok!" << endl;
	//close(sock);
}


int main()
{
	struct sockaddr_in client_addr;
  
	int sock = socket(AF_INET, SOCK_STREAM, 0);
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

	//signal(SIGINT, signal_handler);

	if (listen(sock, MAX_CLIENTS) < 0) {
		close(sock);
		perror("listen failed");
		return EXIT_FAILURE;
	}

	printf("Server running ...");
	
	ThreadPool tp;
	
	while(1) {
		char *buf = (char*)malloc(MESSAGE_LEN);
		socklen_t cli_addr_size = sizeof(client_addr);
		
		int new_sock = accept(sock, (struct sockaddr*) &client_addr, &cli_addr_size);
		if(new_sock < 0) {
			perror_and_exit("accept()", 3);
		}

		int bytes_read = recv(new_sock, buf, MESSAGE_LEN, 0);
		buf[bytes_read] = '\0';
		char tst[MESSAGE_LEN];
		strcpy(tst, buf);

		tp.queueWork(new_sock, tst);
		free(buf);
	}

	close(sock);
	return 0;
}
