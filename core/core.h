/*
 * INTERFACCIA
 */

int send_error(int socket, char *err);
int work_with_threads(int socket, char *client_ip, int port);
int work_with_processes(int socket, char *client_ip, int port);
int write_on_pipe(int size, char* route, int port, char *client_ip);
int start();
char *get_server_ip();
void init(int argc, char *argv[]);
void serve_client(int socket, char *client_ip, int port);
void handle_requests(int port, int (*handle)(int, char*, int));
void *send_routine(void *arg);
int _recv(int s,char *buf,int len,int flags);