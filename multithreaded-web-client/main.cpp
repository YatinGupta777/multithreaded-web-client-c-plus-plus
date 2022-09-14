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
CRITICAL_SECTION queueCriticalSection;
CRITICAL_SECTION hostCriticalSection;
CRITICAL_SECTION ipCriticalSection;

class Parameters {
public:
    queue<char*> links;
    set<DWORD> seen_IP;
    set<string> seen_hosts;
    bool quit;
};

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

bool clean_url(char *&fragment, char *&query, char *&path, char *&port_string, int& port, char  *&host, char* link)
{
    if (strlen(link) > MAX_URL_LEN) {
        printf("URL length exceeds the maximum allowed length %d\n", MAX_URL_LEN);
        return false;
    }

    printf("URL: %s\n", link);
    printf("\tParsing URL... ");
    if (strncmp(link, "http://", 7) != 0)
    {
        printf("failed with invalid scheme\n");
        return false;
    }

    host = link;
    host += 7;

    fragment = extract_and_truncate(host, '#');
    query = extract_and_truncate(host, '?');
    path = extract_and_truncate(host, '/');
    port_string = extract_and_truncate(host, ':');

    port = 80;

    if (port_string != NULL)
    {
        port = atoi(port_string + 1);
        if (port == 0)
        {
            printf("failed with invalid port\n");
            return false;
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
        return false;
    }

    printf("host %s, port %d", host, port);
    printf(", request %s%s", path, query);
    printf("\n");

    return true;
}


void crawl(set<DWORD>& seen_IP, set<string>& seen_hosts, char* link) {
   
    int length = strlen(link) + 1;
    char* original_link = new char[length];
    strcpy_s(original_link, length, link);

    char* host = NULL;
    char* fragment = NULL;
    char* query = NULL;
    char* path = NULL;
    char* port_string = NULL;
    int port = 0;

    char* head_path = new char[12];
    strcpy_s(head_path, 12, "/robots.txt");
    char* head_query = new char[2];
    strcpy_s(head_query, 2, "");

    bool success = clean_url(fragment, query, path, port_string, port, host, link);
    if (success) {

        EnterCriticalSection(&hostCriticalSection);
        auto host_check = seen_hosts.insert(host);
        LeaveCriticalSection(&hostCriticalSection);

        printf("\tChecking host uniqueness...");

        if (true)
        {
            printf("passed\n");
            WebScrapping obj;
            DWORD IP;
            obj.DNS_LOOKUP(host, port, IP);
            printf("\tChecking IP uniqueness...");
            if (!obj.error) {
                EnterCriticalSection(&ipCriticalSection);
                auto ip_result = seen_IP.insert(IP);
                LeaveCriticalSection(&ipCriticalSection);
                if (ip_result.second == false)
                {
                    printf("failed\n");
                    obj.error = true;
                }
            }
            printf("passed\n");

            if (!obj.error) obj.head_request(port, host, head_path, head_query, original_link);
            if (!obj.error) obj.get_request(port, host, path, query, original_link);
            if (!obj.error) obj.parse_response(original_link);
        }
        else {
            printf("failed\n");
        }
    }

    delete[] fragment;
    delete[] query;
    delete[] path;
    delete[] port_string;
    delete[] original_link;
    delete[] head_path;
    delete[] head_query;
}

UINT crawling_thread(LPVOID pParam)
{
    Parameters* p = ((Parameters*)pParam);

    while (true)
    {
        EnterCriticalSection(&queueCriticalSection);
        if (p->links.size() == 0) {
            LeaveCriticalSection(&queueCriticalSection);
            return 0;
        }
        char* link = p->links.front();
        p->links.pop();
        printf("thread %d started\n", GetCurrentThreadId());
        LeaveCriticalSection(&queueCriticalSection);
        crawl(p->seen_IP, p->seen_hosts, link);
    }

    return 0;
}

UINT stats_thread(LPVOID pParam)
{
    Parameters* p = ((Parameters*)pParam);

    while (!p->quit)
    {
        EnterCriticalSection(&queueCriticalSection);
        int size = p->links.size();
        printf("STATS thread %d size %d\n", GetCurrentThreadId(), size);
        LeaveCriticalSection(&queueCriticalSection);
       // Sleep(200);
    }

    printf("DONE\n");

    return 0;
}

bool read_links_from_file(char* filename, queue<char*>&links) {
 
    ifstream file(filename);
    string line;

    if (!file.is_open()) {
        printf("Error reading file\n");
        return true;
    }

    while (getline(file, line)) {
        char* temp = new char[line.length() + 1];
        strcpy(temp, line.c_str());
        links.push(temp);
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
    queue<char*>links;
    HANDLE* handles = NULL;
    int threads;
    Parameters p;

    p.quit = false;

    if (argc == 2) {
        char* a = argv[1];
        links.push(a);
    }
    else if (argc == 3)
    {
        char* temp = argv[1];
        threads = atoi(temp);
        if (threads < 1) {
            printf("Please pass valid number of threads and name of file as 2 arguments\n");
            return 0;  
        }
        handles = new HANDLE[threads];
        if (!InitializeCriticalSectionAndSpinCount(&queueCriticalSection,
            0x00000400) || !InitializeCriticalSectionAndSpinCount(&hostCriticalSection,
                0x00000400) || !InitializeCriticalSectionAndSpinCount(&ipCriticalSection,
                    0x00000400))
            return 0;
        char* filename = argv[2];
        bool error = read_links_from_file(filename, links);
        if(error) return 0;
        p.links = links;
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

    HANDLE statsThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)stats_thread, &p, 0, NULL);

    for (int i = 0; i < threads; i++)
    {
        handles[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)crawling_thread, &p, 0, NULL);
    }
    // make sure this thread hangs here until the other three quit; otherwise, the program will terminate prematurely
    for (int i = 0; i < threads; i++)
    {
        WaitForSingleObject(handles[i], INFINITE);
        CloseHandle(handles[i]);
    }

    p.quit = true;

    printf("Reach1\n");

    WaitForSingleObject(stats_thread, INFINITE);
   // CloseHandle(stats_thread);

    printf("Reach2\n");

   DeleteCriticalSection(&queueCriticalSection);
   DeleteCriticalSection(&hostCriticalSection);
   DeleteCriticalSection(&ipCriticalSection);
    
   WSACleanup();
}

