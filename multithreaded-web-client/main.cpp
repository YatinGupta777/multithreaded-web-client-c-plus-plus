/*
    Yatin Gupta
    Class 612 : Networks and distributed processing
    1st Semester
    yatingupta@tamu.edu
*/

#include "pch.h"
#include <iostream>
#include "WebScrapping.h"

using namespace std;

class WebScrapping;

char* extract_and_truncate(char* link, char c)
{
    char* result = NULL;
    char* temp = strchr(link, c);
    if (temp != NULL) {
        int length = strlen(temp) + 1;
        result = new char[length];
        strcpy_s(result, length, temp);
        *temp = '\0';
    }
    return result;
}

bool read_links_from_file(char* filename, vector<char*>&links) {
 
    ifstream file(filename);
    string line;

    if (!file.is_open()) {
        printf("Error reading file\n");
        return true;
    }

    while (getline(file, line)) {
        char* temp = new char[line.length() + 1];
        strcpy(temp, line.c_str());
        links.push_back(temp);
    }

    //Getting file size
    //TODO: figure out out to get filesize with in one go
    file.clear();
    file.seekg(0, ios::end);
    int file_size = file.tellg();

    printf("Opened %s with size %d\n", filename, file_size);

    file.close();

    return false;
}

int main(int argc, char** argv)
{
    vector<char*>links;
    set<DWORD> seen_IP;
    set<string> seen_hosts;

    if (argc == 2) {
        char* a = argv[1];
        links.push_back(a);
    }
    else if (argc == 3)
    {
        char* temp = argv[1];
        int threads = atoi(temp);
        if (threads != 1) {
            printf("Please pass only 1 thread and name of file as 2 arguments\n");
            return 0;  
        }
        char* filename = argv[2];
        bool error = read_links_from_file(filename, links);
        if(error) return 0;
    }
    else {
        printf("Please pass only URL in format -> scheme://host[:port][/path][?query][#fragment]\n");
        printf("OR\n");
        printf("Please pass only 1 thread and name of file as 2 arguments\n");
        return 0;
    }

    WSADATA wsaData;

    //Initialize WinSock; once per program run 
    WORD wVersionRequested = MAKEWORD(2, 2);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
        printf("WSAStartup failed with %d\n", WSAGetLastError());
        WSACleanup();
        return 0;
    }

    int n = links.size();
    for (int i = 0; i < n; i++)
    {
        char* link = links[i];
        
        if (strlen(link) > MAX_URL_LEN) {
            printf("URL length exceeds the maximum allowed length %d\n", MAX_URL_LEN);
            continue;
        }

        printf("URL: %s\n", link);
        printf("\tParsing URL... ");
        if (strncmp(link, "http://", 7) != 0)
        {
            printf("failed with invalid scheme\n");
            continue;
        }

        int length = strlen(link) + 1;
        char* original_link = new char[length];
        strcpy_s(original_link, length, link);
        char* host = link;
        host += 7;

        char* fragment = extract_and_truncate(host, '#');
        char* query = extract_and_truncate(host, '?');
        char* path = extract_and_truncate(host, '/');
        char* port_string = extract_and_truncate(host, ':');

        int port = 80;

        if (port_string != NULL)
        {
            port = atoi(port_string + 1);
            if (port == 0)
            {
                printf("failed with invalid port\n");
                continue;
            }
        }
        if (path == NULL) {
            int length = 2;
            path = new char[length];
            strcpy_s(path, length, "/");
        };
        if (query == NULL) {
            int length = 2;
            query = new char[length];
            strcpy_s(query, length, "");
        };

        if (strlen(host) > MAX_HOST_LEN) {
            printf("host length exceeds the maximum allowed length %d\n", MAX_HOST_LEN);
            continue;
        }

        printf("host %s, port %d", host, port);
        if(argc==2) printf(", request %s%s", path, query);
        printf("\n");

        char* head_path = new char[12];
        strcpy_s(head_path, 12, "/robots.txt");
        char* head_query = new char[2];
        strcpy_s(head_query, 2, "");
        auto host_check = seen_hosts.insert(host);

        printf("\tChecking host uniqueness...");

        if (host_check.second == true)
        {
            printf("passed\n");
            WebScrapping obj;
            obj.DNS_LOOKUP(host, port, seen_IP);

            if (!obj.error) obj.head_request(port, host, head_path, head_query, original_link);
            if (!obj.error) obj.get_request(port, host, path, query, original_link);
            if (!obj.error) obj.parse_response(original_link);
        }
        else {
            printf("failed\n");
        }

        delete[] fragment;
        delete[] query;
        delete[] path;
        delete[] port_string;
        delete[] original_link;
        delete[] head_path;
        delete[] head_query;
    }
    WSACleanup();
}

