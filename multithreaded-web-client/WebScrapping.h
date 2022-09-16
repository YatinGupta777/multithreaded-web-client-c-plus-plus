#pragma once
#include "pch.h"

class WebScrapping
{
public:
	WebScrapping();
	~WebScrapping();

	char* get_buffer;
	int get_buffer_size;

	char* head_buffer;
	int head_buffer_size;
	struct sockaddr_in server;
	clock_t start_t;
	clock_t end_t;
	bool error;
	bool print;
	void cleanup(HANDLE event, SOCKET sock);
	int connect_and_parse(char*& buffer, int port, const char* method, char* host, char* path, char* query, char* link, int status_code_validation, int &buffer_size, int max_size, const char* http_version);
	SOCKET connect_socket(char* host, int port, const char* method);
	void head_request(int port, char* host, char* path, char* query, char* link);
	int get_request(int port, char* host, char* path, char* query, char* link);
	int get_request_HTTP_1(int port, char* host, char* path, char* query, char* link);
	void DNS_LOOKUP(char* host, int port);
	int parse_response(char* link, HTMLParserBase*& parser); 
	int parseHTTP(char* link, HTMLParserBase*& parser);
	void read_data(HANDLE event, SOCKET sock, char*& buffer, int& curr_pos, int max_size);
	bool clean_url(char*& fragment, char*& query, char*& path, char*& port_string, int& port, char*& host, char* link);
};