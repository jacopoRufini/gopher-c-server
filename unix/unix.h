#include <arpa/inet.h>
#include <sys/mman.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include "../shared/shared.h"
#include CONSTANTS
#include CORE_PATH

int pipe_fd[2];

pthread_attr_t pthread_attr;
pthread_mutex_t *mutex;
pthread_cond_t *condition;

int _log(char *buffer);
void restart();
void daemon_skeleton();
void *listener_routine(void *arg);
void *receiver_routine(void *arg);
void log_routine();
int listen_on(int port, int *socket_fd, struct sockaddr_in *socket_addr);