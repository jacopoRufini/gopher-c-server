#include "unix.h"
extern configuration *config;

void restart() {
    if (load_configuration(PORT_ONLY) != -1) {
        if (start() < 0) {
            perror("Impossibile fare il restart del server. \n");
        }
    }
}

int is_daemon(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        char *input = argv[i];
        if (strncmp(input, DAEMON_FLAG, strlen(DAEMON_FLAG)) == 0) {
            return 1;
        }
    }
    return 0;
}

void daemon_skeleton() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Errore durante la fork. (daemon_skeleton)\n");
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }
    if (umask(0) < 0) {
        perror("Errore umask(0).\n");
        exit(1);
    }
    if (setsid() < 0) {
        perror("Errore setsid().\n");
        exit(1);
    }

    if (close(STDIN_FILENO) < 0) {
        perror("Impossibile chiudere il file descriptor: standard input. \n");
        exit(1);
    }
    if (close(STDOUT_FILENO) < 0) {
        perror("Impossibile chiudere il file descriptor: standard output. \n");
        exit(1);
    }
    if (close(STDERR_FILENO) < 0) {
        perror("Impossibile chiudere il file descriptor: standard error. \n");
        exit(1);
    }
}

void init(int argc, char *argv[]) {
    if (is_daemon(argc, argv)) {
        daemon_skeleton();
    }

    if (pipe(pipe_fd) == -1) {
        perror("Errore durante l'inizializzazione della pipe.\n");
        exit(1);
    }

    config = mmap(NULL, sizeof config, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (config == MAP_FAILED) {
        perror("Errore nell'operazione di mapping del file di configurazione.\n");
        exit(1);
    }

    mutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mutex == MAP_FAILED) {
        perror("Errore nell'operazione di mapping del mutex.\n");
        exit(1);
    }

    condition = mmap(NULL, sizeof(pthread_cond_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (condition == MAP_FAILED) {
        perror("Errore nell'operazione di mapping della condition variable.\n");
        exit(1);
    }

    if (pthread_attr_init(&pthread_attr) != 0) {
        perror("Errore nell'inizializzazione degli attributi del thread.\n");
        exit(1);
    }

    if (pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("Errore nel settare DETACHED_STATE al thread.\n");
        exit(1);
    }

    if (load_configuration(COMPLETE) == -1 || load_arguments(argc,argv) == -1) {
        exit(1);
    } config->main_pid = getpid();

    pthread_mutexattr_t mutexAttr = {};
    if (pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED)) {
        perror("Impossibile impostare l'attributo del mutex: PTHREAD_SHARED.\n");
        exit(1);
    }
    if (pthread_mutex_init(mutex, &mutexAttr)) {
        perror("Impossibile inizializzare l'attributo del mutex.\n");
        exit(1);
    }

    pthread_condattr_t condAttr = {};
    if (pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED)) {
        perror("Impossibile impostare l'attributo della condition variable: PTHREAD_SHARED.\n");
        exit(1);
    }
    if (pthread_cond_init(condition, &condAttr)) {
        perror("Impossibile inizializzare l'attributo della condition variable.\n");
        exit(1);
    }

    pid_t pid_child = fork();
    if (pid_child < 0) {
        perror("Errore durante la fork. (log routine)\n");
        exit(1);
    } else if (pid_child == 0) {
        log_routine();
        exit(0);
    }

    if (start() < 0) {
        perror("Impossibile avviare il server. \n");
        exit(1);
    }

    if (write_infos() == -1) {
        exit(1);
    }

    if (signal(SIGHUP, restart) == SIG_ERR) {
        perror("Impossibile catturare il segnale SIGHUP. (restart)\n");
    }
    while(1) sleep(1);
}

void log_routine() {
    char buffer[LOG_SIZE];
    while(1) {
        bzero(buffer, LOG_SIZE);
        pthread_mutex_lock(mutex);
        pthread_cond_wait(condition, mutex);
        if (read(pipe_fd[0], buffer, sizeof buffer) < 0) {
            perror("Errore durante la read sulla pipe.\n");
            pthread_mutex_unlock(mutex);
            continue;
        }
        if (_log(buffer) < 0) {
            perror("Errore nell'operazione di scrittura sul log.\n");
        }
        pthread_mutex_unlock(mutex);
    }
}

int write_on_pipe(int size, char* route, int port, char *client_ip) {
    int min_size = strlen("name: ") + strlen(" | size: ") + strlen(" | ip: ") + strlen(" | server_port: ") + 2;
    char buffer[strlen(route) + strlen(client_ip) + MAX_PORT_LENGTH + min_size];
    sprintf(buffer, "name: %s | size: %d | ip: %s | server_port: %d\n", route, size, client_ip, port);
    pthread_mutex_lock(mutex);
    if (write(pipe_fd[1], buffer, strlen(buffer)) < 0) {
        return -1;
    }
    if (pthread_cond_signal(condition) != 0) {
        perror("Impossibile inviare il segnale. (write_on_pipe - pthread_cond_signal");
    }
    pthread_mutex_unlock(mutex);
    return 0;
}

int listen_on(int port, int *socket_fd, struct sockaddr_in *socket_addr) {
    bzero(socket_addr, sizeof *socket_addr);
    socket_addr->sin_family = AF_INET;
    socket_addr->sin_port = htons(port);
    socket_addr->sin_addr.s_addr = INADDR_ANY;

    if ((*socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Errore durante la creazione della socket.\n");
        return -1;
    } if (bind(*socket_fd, (struct sockaddr *)socket_addr, sizeof *socket_addr) == -1) {
        perror("Errore durante il binding tra socket_fd e socket_address.\n");
        return -1;
    } if (listen(*socket_fd, BACKLOG) == -1) {
        perror("Errore nel provare ad ascoltare sulla porta data in input.\n");
        return -1;
    }
    return 0;
}

char *get_server_ip() {
    struct ifaddrs *addrs;
    int res = getifaddrs(&addrs);
    struct ifaddrs *tmp = addrs;
    char *ip = (char*)malloc(MAX_IP_LENGTH + 1);
    if (ip == NULL) {
        perror("Errore durante la malloc. (get_server_ip)\n");
        freeifaddrs(addrs);
        return ip;
    }

    if (res < 0) {
        freeifaddrs(addrs);
        return ip;
    }

    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
            strcpy(ip, inet_ntoa(pAddr->sin_addr));
            break;
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(addrs);
    return ip;
}

int send_error(int socket, char *err) {
    return send(socket, err, strlen(err), 0);
}

int start() {
    if (equals(config->server_type, "thread")) {
        pthread_t thread;
        if (pthread_create(&thread,&pthread_attr,listener_routine, (void *) &config->server_port) != 0) {
            perror("Impossibile creare il thread. (listener)\n");
            return -1;
        }
    } else {
        pid_t pid_child = fork();
        if (pid_child < 0) {
            perror("Errore durante la fork. (start)\n");
            return -1;
        } else if (pid_child == 0) {
            handle_requests(config->server_port, work_with_processes);
            exit(0);
        }
    }
    return 0;
}

void release() {
    pthread_exit(NULL);
}

void *listener_routine(void *arg) {
    signal(SIGINT, release);
    int port = *(int*)arg;
    handle_requests(port, work_with_threads);
    return NULL;
}

void handle_requests(int port, int (*handle)(int, char*, int)) {
    int server_socket;
    fd_set active_fd_set, read_fd_set;
    struct sockaddr_in server_addr, client_addr;

    if (listen_on(port, &server_socket, &server_addr) != 0) {
        return;
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    FD_ZERO (&active_fd_set);
    FD_SET (server_socket, &active_fd_set);

    while (1) {
        read_fd_set = active_fd_set;

        int selected;
        if ((selected = select(FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout)) < 0) {
            perror("Errore durante l'operazione di select.\n");
            return;
        }

        if(config->server_port != port) {
            printf("Chiusura socket su porta %d.\n", port);
            if (close(server_socket) < 0) {
                perror("Impossibile chiudere il file descriptor. (server_socket)\n");
            }
            return;
        }

        if (selected == 0) continue;

        for (int fd = 0; fd < FD_SETSIZE; fd++) {
            if (FD_ISSET (fd, &read_fd_set)) {
                if (fd == server_socket) {
                    socklen_t size = sizeof(client_addr);
                    int new = accept(server_socket, (struct sockaddr *) &client_addr, &size);
                    if (new < 0) {
                        perror("Errore durante l'accept del client.\n");
                        continue;
                    }
                    FD_SET (new, &active_fd_set);
                } else {
                    char client_ip[INET_ADDRSTRLEN];
                    if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN) == NULL) {
                        perror("Impossibile ottenere l'indirizzo IP del client. \n");
                    }
                    if (handle(fd, client_ip, port) == -1) {
                        FD_CLR (fd, &read_fd_set);
                        if (close(fd) < 0) {
                            perror("Impossibile chiudere il file descriptor. (client_socket)\n");
                        }
                        fprintf(stderr, "Errore nel comunicare con la socket %d. Client-ip: %s\n", fd, client_ip);
                        continue;
                    }
                    FD_CLR (fd, &active_fd_set);
                }
            }
        }
    }
}

int work_with_threads(int socket, char *client_ip, int port) {
    thread_arg_receiver *args = (thread_arg_receiver *)malloc(sizeof(thread_arg_receiver));
    if (args == NULL) {
        perror("Impossibile allocare memoria per gli argomenti del thread di tipo 'receiver'.\n");
        return -1;
    }
    args->client_socket = socket;
    args->port = port;
    args->client_ip = client_ip;

    pthread_t thread;
    if (pthread_create(&thread, &pthread_attr, receiver_routine, (void *)args) != 0) {
        perror("Impossibile creare un nuovo thread di tipo 'receiver'.\n");
        free(args);
        return -1;
    }
    return 0;
}

int work_with_processes(int socket, char *client_ip, int port) {
    pid_t pid_child = fork();
    if (pid_child < 0) {
        perror("Errore durante la fork. (work_with_processes)\n");
        return -1;
    } else if (pid_child == 0) {
        serve_client(socket, client_ip, port);
        exit(0);
    } else {
        close(socket);
    }
    return 0;
}

void *receiver_routine(void *arg) {
    thread_arg_receiver *args = (thread_arg_receiver *) arg;
    serve_client(args->client_socket, args->client_ip, args->port);
    free(arg);
    return NULL;
}

void *send_routine(void *arg) {
    thread_arg_sender *args = (thread_arg_sender *) arg;
    if (send(args->client_socket, args->file_in_memory, args->size, 0) < 0) {
        fprintf(stderr, "Errore nel comunicare con la socket. ('sender')\n");
    } else {
        if (write_on_pipe(args->size, args->route, args->port, args->client_ip) < 0) {
            fprintf(stderr, "Errore scrittura su pipe. ('sender')\n");
        }
    }
    if (args->size > 0) {
        if (munmap(args->file_in_memory, args->size) == -1) {
            perror("Impossibile eseguire l'unmapping del file.\n");
        }
    }
    if (close(args->client_socket) < 0) {
        perror("Impossibile chiudere il file descriptor. (send_routine - client_socket)\n");
    }
    free(args->route);
    free(arg);
    return NULL;
}

void serve_client(int socket, char *client_ip, int port) {
    int n;
    char *client_buffer = get_client_buffer(socket, &n);

    if (n < 0) {
        char *err = "Errore nel ricevere i dati o richiesta mal posta.\n";
        fprintf(stderr,"%s",err);
        if (send_error(socket, err) < 0) {
            perror("Impossibile mandare errore al client. (serve_client - send_error)\n");
        }
        if (close(socket) < 0) {
            perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
        }
        return;
    }

    char path[strlen(PUBLIC_PATH) + strlen(client_buffer) + 1];
    sprintf(path,"%s%s", PUBLIC_PATH, client_buffer);

    int file_type = is_file(path);
    if (file_type > 0) {
        int file_fd = open(path, O_RDONLY);
        char *err = "Errore nell'apertura del file richiesto.\n";
        if (file_fd == -1) {
            fprintf(stderr,"%s",err);
            if (send_error(socket, err) < 0) {
                perror("Impossibile mandare errore al client. (serve_client - send_error)\n");
            }
            if (close(socket) < 0) {
                perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
            }
            free(client_buffer);
            return;
        }

        struct stat v;
        if (stat(path,&v) == -1) {
            perror("Errore nel prendere la grandezza del file.\n");
            if (send_error(socket,err) < 0) {
                perror("Impossibile mandare errore al client. (serve_client - send_error)\n");
            }
            if (close(socket) < 0) {
                perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
            }
            free(client_buffer);
            return;
        }

        char *file_in_memory;
        int size = v.st_size;
        if (size > 0) {
            if (flock(file_fd, LOCK_EX) < 0) {
                perror("Impossibile lockare il file.\n");
                free(client_buffer);
                if (close(socket) < 0) {
                    perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
                }
                return;
            }
            file_in_memory = mmap(NULL, size, PROT_READ, MAP_PRIVATE, file_fd, 0);
            if (flock(file_fd, LOCK_UN) < 0) {
                perror("Impossibile unlockare il file.\n");
                free(client_buffer);
                if (close(socket) < 0) {
                    perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
                }
                return;
            }

            if (file_in_memory == MAP_FAILED) {
                perror("Errore nell'operazione di mapping del file.\n");
                free(client_buffer);
                if (close(socket) < 0) {
                    perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
                }
                return;
            }
        } else {
            file_in_memory = "";
        }

        if (close(file_fd) < 0) {
            perror("Impossibile chiudere il file descriptor. (serve_client - file richiesto)\n");
        }

        thread_arg_sender *args = (thread_arg_sender *)malloc(sizeof (thread_arg_sender));

        if (args == NULL) {
            perror("Impossibile allocare memoria per gli argomenti del thread di tipo 'sender'.\n");
            if (close(socket) < 0) {
                perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
            }
            free(client_buffer);
            return;
        }

        args->client_ip = client_ip;
        args->client_socket = socket;
        args->port = port;
        args->file_in_memory = file_in_memory;
        args->route = client_buffer;
        args->size = size;

        if (equals(config->server_type, "thread")) {
            send_routine(args);
        } else if (equals(config->server_type, "process")) {
            pthread_t thread;
            if (pthread_create(&thread, NULL, send_routine, (void *) args) != 0) {
                perror("Impossibile creare un nuovo thread di tipo 'sender'.\n");
                if (close(socket) < 0) {
                    perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
                }
                free(client_buffer);
                free(args);
                return;
            }
            pthread_join(thread,NULL);
        }
    } else if (file_type == 0){
        char *listing_buffer;
        if ((listing_buffer = get_file_listing(client_buffer, path)) == NULL) {
            if (close(socket) < 0) {
                perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
            }
            free(listing_buffer);
            free(client_buffer);
            return;
        }
        else {
            if (send(socket, listing_buffer, strlen(listing_buffer), 0) < 0) {
                perror("Errore nel comunicare con la socket.\n");
                if (close(socket) < 0) {
                    perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
                }
                free(listing_buffer);
                free(client_buffer);
                return;
            }
        }
        free(listing_buffer);
        free(client_buffer);
        if (close(socket) < 0) {
            perror("Impossibile chiudere il file descriptor. (serve_client - socket)\n");
        }
    } else {
        if (send_error(socket, "File o directory non esistente.") < 0) {
            perror("Impossibile mandare errore al client. (serve_client - send_error)\n");
        }
        free(client_buffer);
        close(socket);
    }
}

int _log(char *buffer) {
    FILE *file = fopen(LOG_PATH, "a");
    if (file == NULL) {
        return -1;
    }
    fprintf(file, "%s", buffer);
    if (fclose(file) == EOF) {
        perror("Errore durante la chiusura del file di log.\n");
    }
    return 0;
}

int _recv(int s,char *buf,int len,int flags) {
    return recv(s,buf,len,flags);
}
