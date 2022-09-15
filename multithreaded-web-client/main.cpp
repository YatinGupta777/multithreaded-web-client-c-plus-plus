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
CRITICAL_SECTION activeThreadsCriticalSection;
CRITICAL_SECTION statsCriticalSection;

class Parameters {
public:
    queue<char*> links;
    set<DWORD> seen_IP;
    set<string> seen_hosts;
    int active_threads;
    int extracted_urls;
    int unique_hosts;
    int dns_lookups;
    int unique_ips;
    int robot_checks;
    vector<int>status_codes;
    int total_links_found;
    HANDLE	eventQuit;
    int pages;
    int bytes;
};

void crawl(Parameters*p, char* link) {
   
    WebScrapping obj;
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

    bool success = obj.clean_url(fragment, query, path, port_string, port, host, link);
    if (success) {

        EnterCriticalSection(&statsCriticalSection);
        p->extracted_urls++;
        LeaveCriticalSection(&statsCriticalSection);

        EnterCriticalSection(&hostCriticalSection);
        auto host_check = p->seen_hosts.insert(host);
        LeaveCriticalSection(&hostCriticalSection);
        
        if(obj.print) printf("\tChecking host uniqueness...");

        if (host_check.second == true)
        {
            EnterCriticalSection(&statsCriticalSection);
            p->unique_hosts++;
            LeaveCriticalSection(&statsCriticalSection);
            if (obj.print) printf("passed\n");
            DWORD IP;
            obj.DNS_LOOKUP(host, port, IP);
            
            if (!obj.error) {

                EnterCriticalSection(&statsCriticalSection);
                p->dns_lookups++;
                LeaveCriticalSection(&statsCriticalSection);

                if (obj.print) printf("\tChecking IP uniqueness...");
                EnterCriticalSection(&ipCriticalSection);
                auto ip_result = p->seen_IP.insert(IP);
                LeaveCriticalSection(&ipCriticalSection);

                if (ip_result.second == true)
                {
                    EnterCriticalSection(&statsCriticalSection);
                    p->unique_ips++;
                    LeaveCriticalSection(&statsCriticalSection);
                    if (obj.print) printf("passed\n");
                }
                else {
                    if (obj.print) printf("failed\n");
                    obj.error = true;
                }   
            }
            if (!obj.error) {
                obj.head_request(port, host, head_path, head_query, original_link);
                EnterCriticalSection(&statsCriticalSection);
                p->bytes += obj.head_buffer_size;
                LeaveCriticalSection(&statsCriticalSection);
            }

            EnterCriticalSection(&statsCriticalSection);
            p->robot_checks++;
            LeaveCriticalSection(&statsCriticalSection);

            if (!obj.error) {
                int code = obj.get_request(port, host, path, query, original_link);
                EnterCriticalSection(&statsCriticalSection);
                if (code >= 200 && code < 300) p->status_codes[0]++;
                else if (code >= 300 && code < 400) p->status_codes[1]++;
                else if (code >= 400 && code < 500) p->status_codes[2]++;
                else if (code >= 500 && code < 600) p->status_codes[3]++;
                else if (code != -1) p->status_codes[4]++;                
                p->pages++;
                p->bytes += obj.get_buffer_size;
                LeaveCriticalSection(&statsCriticalSection);
            }

            if (!obj.error) {
                int nlinks = obj.parse_response(original_link);
                p->total_links_found += nlinks;
            }
        }
        else {
            if (obj.print) printf("failed\n");
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
            EnterCriticalSection(&activeThreadsCriticalSection);
            p->active_threads--;
            LeaveCriticalSection(&activeThreadsCriticalSection);
            return 0;
        }
        char* link = p->links.front();
        p->links.pop();
        LeaveCriticalSection(&queueCriticalSection);
        crawl(p, link);
    }

    return 0;
}

UINT stats_thread(LPVOID pParam)
{
    Parameters* p = ((Parameters*)pParam);
    clock_t start, small_start, small_end;
    start = clock();
    small_start = clock();
    while (WaitForSingleObject(p->eventQuit, 2000) == WAIT_TIMEOUT)
    {
        EnterCriticalSection(&queueCriticalSection);
        int size = p->links.size();
        LeaveCriticalSection(&queueCriticalSection);
        int elapsed_time = clock() - start;
        EnterCriticalSection(&activeThreadsCriticalSection);
        int active_threads = p->active_threads;
        LeaveCriticalSection(&activeThreadsCriticalSection);

        EnterCriticalSection(&statsCriticalSection);
        int extracted_urls = p->extracted_urls;
        int unique_hosts = p->unique_hosts;
        int dns_lookups = p->dns_lookups;
        int unique_ips = p->unique_ips;
        int robot_checks = p->robot_checks;
        int crawled_urls = p->status_codes[0];
        int total_links_found = p->total_links_found;
        int pages = p->pages;
        int bytes = p->bytes;
        p->pages = 0;
        p->bytes = 0;
        LeaveCriticalSection(&statsCriticalSection);

        small_end = clock();
        int d = (small_end - small_start) / 1000;

        float speed = (((float)bytes * 8.0 / 1000000.0)) / (float) d;
        printf("[%3d] %d Q %d E %3d H %3d D %3d I %3d R %3d C %3d L %3d\n", elapsed_time/1000, active_threads, size, extracted_urls, unique_hosts, dns_lookups, unique_ips, robot_checks, crawled_urls, total_links_found);
        printf("*** pages %d bytes %d crawling %d pps @ %.2f Mbps\n", pages, bytes, pages / d, speed);
        small_start = clock();
    }

    int elapsed_time = clock() - start;

    printf("Extracted %d URLS @ %d/s\n", p->extracted_urls, p->extracted_urls / elapsed_time);
    printf("Looked up %d DNS names @ %d/s\n", p->dns_lookups, p->dns_lookups / elapsed_time);
    printf("Attempted %d robots @ %d/s\n", p->robot_checks, p->robot_checks / elapsed_time);
    printf("Crawled %d pages @ %d/s\n", p->status_codes[0], p->status_codes[0] / elapsed_time);
    printf("Parsed %d links @ %d/s\n", p->total_links_found, p->total_links_found / elapsed_time);
    printf("HTTP codes: 2xx = %d, 3xx = %d, 4xx = %d, 5xx = %d, other = %d\n", p->status_codes[0], p->status_codes[1], p->status_codes[2], p->status_codes[3], p->status_codes[4]);

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
                    0x00000400) || !InitializeCriticalSectionAndSpinCount(&activeThreadsCriticalSection,
                        0x00000400) || !InitializeCriticalSectionAndSpinCount(&statsCriticalSection,
                            0x00000400))
            return 0;
        
        char* filename = argv[2];
        bool error = read_links_from_file(filename, links);
        if(error) return 0;
        p.links = links;
        p.extracted_urls = 0;
        p.unique_hosts = 0;
        p.dns_lookups = 0;
        p.unique_ips = 0;
        p.robot_checks = 0;
        p.total_links_found = 0;
        p.status_codes = { 0,0,0,0,0 };
        p.pages = 0;
        p.eventQuit = CreateEvent(NULL, true, false, NULL);
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

    HANDLE stats_thread_handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)stats_thread, &p, 0, NULL);
    p.active_threads = threads;

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

    SetEvent(p.eventQuit);

    WaitForSingleObject(stats_thread_handle, INFINITE);
    CloseHandle(stats_thread_handle);

   DeleteCriticalSection(&queueCriticalSection);
   DeleteCriticalSection(&hostCriticalSection);
   DeleteCriticalSection(&ipCriticalSection);
   DeleteCriticalSection(&activeThreadsCriticalSection); 
   DeleteCriticalSection(&statsCriticalSection);

   WSACleanup();
}

