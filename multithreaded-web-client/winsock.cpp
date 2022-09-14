/* winsock.cpp
 * DNS lookup code
 * by Dmitri Loguinov
 */
#include "pch.h"
#include "WebScrapping.h"
#pragma comment(lib, "ws2_32.lib")
#define INITIAL_BUF_SIZE 4096

int html_parser(char* html_code, char* link, int html_content_length);

WebScrapping :: WebScrapping(){
	get_buffer = NULL;
	head_buffer = NULL;
	error = false;
	get_buffer_size = 0;
	head_buffer_size = 0;
	start_t = clock();
	end_t = clock();
}

WebScrapping:: ~WebScrapping() {
	free(get_buffer);
	free(head_buffer);
}

void WebScrapping::cleanup(HANDLE event, SOCKET sock)
{
	WSACloseEvent(event);
	closesocket(sock);
}

void WebScrapping::DNS_LOOKUP(char* host, int port, DWORD& IP) {
	start_t = clock();
	printf("\tDoing DNS... ");

	// structure used in DNS lookups 
	struct hostent* remote;

	// first assume that the string is an IP address
	IP = inet_addr(host);
	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((remote = gethostbyname(host)) == NULL)
		{
			printf("failed with %d\n", WSAGetLastError());
			error = true;
			return;
		}
		else // take the first IP address and copy into sin_addr
			memcpy((char*)&(server.sin_addr), remote->h_addr, remote->h_length);
	}
	else
	{
		// if a valid IP, directly drop its binary version into sin_addr
		server.sin_addr.S_un.S_addr = IP;
	}
	end_t = clock();
	printf("done in %d ms, found %s\n", (end_t - start_t), inet_ntoa(server.sin_addr));
	
	// setup the port # and protocol type
	server.sin_family = AF_INET;
	server.sin_port = htons(port);		// host-to-network flips the byte order
}

void WebScrapping::read_data(HANDLE event, SOCKET sock, char*& buffer, int& curr_pos, int max_size) {
	WSANETWORKEVENTS network_events;

	int allocated_size = INITIAL_BUF_SIZE;
	buffer = (char*)malloc(INITIAL_BUF_SIZE);

	int time_ends = clock() + 10000;

	while (1)
	{
		int time_remaining = time_ends - clock();
		 
		if (time_remaining < 0) {
			printf("failed with timeout\n");
			error = true;
			return;
		}

		int index = WaitForSingleObject(event, time_remaining);
		int network_event = WSAEnumNetworkEvents(sock, event, &network_events);

		if (network_event == SOCKET_ERROR) {
			printf("failed with WSA network event %d\n", WSAGetLastError());
			error = true;
			return;
		}

		if (index == WAIT_TIMEOUT) {
			printf("failed with timeout\n");
			error = true;
			return;
		}
		if (index == WAIT_FAILED || index == WAIT_ABANDONED) {
			printf("failed in waiting for data %d\n", WSAGetLastError());
			error = true;
			return;
		}
		if (network_events.lNetworkEvents & FD_READ)
		{
			int bytes_received = recv(sock, buffer + curr_pos, allocated_size - curr_pos, 0);

			if (bytes_received == 0) {
				break;
			}
			else if (bytes_received < 0) {
				printf("failed with %d on recv\n", WSAGetLastError());
				error = true;
				return;
			}

			curr_pos += bytes_received;

			if (curr_pos > max_size) {
				printf("failed with exceeding max\n");
				error = true;
				return;
			}

			if ((allocated_size - curr_pos) <= INITIAL_BUF_SIZE)
			{
				allocated_size *= 2;
				char* memory = (char*)realloc(buffer, allocated_size);
				if (memory != NULL) buffer = memory;
				else {
					printf("Problem with allocating memory\n");
					error = true;
					return;
				}
			}
		}
		if (network_events.lNetworkEvents & FD_CLOSE)
		{
			if (network_events.iErrorCode[FD_CLOSE_BIT] != 0)
			{
				printf("FD_CLOSE failed with %d\n", network_events.iErrorCode[FD_CLOSE_BIT]);
				error = true;
				return;
			}
			break;
		}
	}
}

SOCKET WebScrapping::connect_socket(char* host, int port, const char* method) {
	// open a TCP socket
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		printf("socket() generated error %d\n", WSAGetLastError());
		error = true;
		return NULL;
	}

	const char* request_type = "robots";
	const char* symbol = "";

	if (method == "GET") {
		request_type = "page";
		symbol = "* ";
	}

	start_t = clock();
	printf("\t%sConnecting on %s... ", symbol, request_type);

	// connect to the server on port 80
	if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		printf("failed with %d\n", WSAGetLastError());
		error = true;
		return NULL;
	}

	end_t = clock();
	printf("done in %d ms\n", (end_t - start_t));
	return sock;
}

int WebScrapping::connect_and_parse(char*& buffer, int port, const char* method, char* host, char* path, char* query, char* link, int status_code_validation, int &curr_pos, int max_size)
{
	start_t = clock();

	int request_size = MAX_REQUEST_LEN;
	char* request = new char[request_size];

	sprintf_s(request, request_size, "%s %s%s HTTP/1.0\r\nHost:%s\r\nUser-agent: Yat/2.0\r\nConnection: close\r\n\r\n", method, path, query, host);

	SOCKET sock = connect_socket(host, port, method);

	if (error) return -1;

	int send_request = send(sock, request, (int)strlen(request), 0);

	delete[] request;

	if (send_request == SOCKET_ERROR) {
		printf("socket failed with %d\n", WSAGetLastError());
		error = true;
		return 0;
	}

	printf("\tLoading... ");

	// Create new event 
	HANDLE event = CreateEventA(0, FALSE, FALSE, 0);

	if (event == NULL) {
		printf("creating handle failed with %d\n", WSAGetLastError());
		closesocket(sock);
		WSACleanup();
		error = true;
		return -1;
	}	

	int event_select = WSAEventSelect(sock, event, FD_READ | FD_CLOSE);
	if (event_select == SOCKET_ERROR) {
		printf("event failed with %d\n", WSAGetLastError());
		cleanup(event, sock);
		error = true;
		return -1;
	}

	read_data(event, sock, buffer, curr_pos, max_size);
	cleanup(event, sock);

	if (error) return -1;

	buffer[curr_pos] = '\0';
	end_t = clock();
	printf("done in %d ms with %d bytes\n", (end_t- start_t), curr_pos);
	printf("\tVerifying header... ");

	if (strncmp(buffer, "HTTP/", 5) != 0)
	{
		printf("failed with non HTTP-header\n");
		error = true;
		return -1;
	}

	char status_code_string[4];
	memcpy(status_code_string, buffer +9, 3);
	status_code_string[3] = '\0';
	int status_code = atoi(status_code_string);

	if (status_code == 0)
	{
		printf("failed with invalid status code\n");
		error = true;
		return -1;
	}

	printf("status code %d\n", status_code);

	if (status_code < status_code_validation || status_code >= status_code_validation + 100) {
		error = true;
		return -1;
	}

	return status_code;
}

void WebScrapping::head_request(int port, char* host, char* path, char* query, char* link) {
	if(!error) connect_and_parse(head_buffer, port, "HEAD", host, path, query, link, 400, head_buffer_size, 16000);
}

int WebScrapping::get_request(int port, char* host, char* path, char* query, char* link) {
	if (!error) return connect_and_parse(get_buffer, port, "GET", host, path, query, link, 200, get_buffer_size, 2000000);
	return -1;
}

int  WebScrapping::parse_response(char* link) {
	
	char* html_content = strstr(get_buffer, "\r\n\r\n");
	int html_content_length = 0;
	int header_length = get_buffer_size;

	if (html_content != NULL) {
		header_length = html_content - get_buffer;
		html_content_length = get_buffer_size - header_length;
	}

	char* header_response = new char[header_length + 1];
	memcpy(header_response, get_buffer, header_length);
	header_response[header_length] = '\0';
	int nlinks = html_parser(html_content, link, html_content_length);
	printf("\t+ Parsing page... ");
	start_t = clock();
	end_t = clock();
	printf("done in %d ms with %d links\n", (end_t - start_t), nlinks);
	return nlinks;
}

//http://tamu.edu/
//http://www.tamu.edu:80
//http://128.194.135.72/courses/index.asp#location
//http://165.91.22.70/
//http://s2.irl.cs.tamu.edu/IRL7
//http://s2.irl.cs.tamu.edu/IRL8
//http://facebook.com:443/?addrbook.php
//http://relay.tamu.edu:465/index.html
//http://ftp.gnu.org:21/
//http://s22.irl.cs.tamu.edu:990/view?test=1
//http://128.194.135.11/?viewcart.php/

//http://www.dmoz.org/
//http://128.194.135.72/
//http://www.yahoo.com/
//http://facebook.com:443/?addrbook.php
//http://relay.tamu.edu:465/index.html
//http://128.194.135.25/?viewcart.php/
//http://s22.irl.cs.tamu.edu:990/view?test=1
//http://ftp.gnu.org:21/
//http://xyz.com:/
//https://yahoo.com/
//http://info.cern.ch/