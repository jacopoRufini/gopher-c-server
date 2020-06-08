#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#define CONSTANTS "../core/constants.h"
#include CONSTANTS
#include CORE_PATH

#define equals(str1, str2) strcmp(str1,str2) == 0

#if defined(__linux__) || defined(__APPLE__)
    #define UNIX_OS 1
#elif defined(_WIN32)
    #define UNIX_OS 0
#endif


typedef struct configuration {
    char server_type[MAX_TYPE_LENGTH+1];
    char server_ip[MAX_IP_LENGTH + 1];
    unsigned int server_port;
    int main_pid;
} configuration;

typedef struct thread_arg_sender {
    int size;
    int client_socket;
    int port;
    char *file_in_memory;
    char *route;
    char *client_ip;
} thread_arg_sender;

typedef struct thread_arg_receiver {
    int client_socket;
    int port;
    char *client_ip;
} thread_arg_receiver;

configuration conf;

int load_arguments(int argc, char *argv[]);
int load_configuration(int first_start);
int index_of(char *values, char find);
int is_file(char *path);
int is_daemon(int argc, char *argv[]);
int write_infos();
char *get_extension_code(const char *filename);
char *get_parameter(char *line, FILE *stream);
char *get_client_buffer(int socket, int *n);
char *get_file_listing(char *route, char *path);

