#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <windows.h>
#include <tchar.h>

#include "../shared/shared.h"

#include CONSTANTS
#include CORE_PATH

#pragma comment(lib,"ws2_32.lib")
#define BUF_SIZE 256
#define PIPENAME "\\\\.\\pipe\\LogPipe"
#define GLOBAL_MUTEX "Global\\Mutex"
#define GLOBAL_CONFIG "Global\\Config"
#define LOGGER_EVENT   "Logger_Event"
#define PIPE_EVENT   "Pipe_Event"

STARTUPINFO info;
PROCESS_INFORMATION listener_info;

HANDLE pipe_event;
HANDLE logger_event;
HANDLE mutex;
HANDLE map_handle;

typedef struct thread_arg_controller {
    SOCKET socket;
    HANDLE hProcess;
} thread_arg_controller;

int set_shared_config();
int get_shared_config();
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);
DWORD WINAPI listener_routine(void *args);
DWORD WINAPI receiver_routine(void *args);
DWORD WINAPI sender_routine(void *args);
DWORD WINAPI controller_routine(void *args);
PROCESS_INFORMATION create_receiver_process(char *args);
int listen_on(int port, struct sockaddr_in *server, int *addrlen, SOCKET *sock);
int restart();
void print_error(char *err);
void print_WSA_error(char *err);