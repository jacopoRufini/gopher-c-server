#include <stdio.h>
#include "win.h"

extern configuration *config;
BOOL WINAPI CtrlHandler2(DWORD fdwCtrlType);

int main(int argc, char *argv[]) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0) {
        print_WSA_error("Impossibile avviare la Winsock DLL (listener - main)");
        exit(1);
    }

    if (SetConsoleCtrlHandler(CtrlHandler2, TRUE) == 0) {
        print_error("Impossibile aggiungere la funzione di handling per CTRL+BREAK (listener - SetConsoleCtrlHandler)");
        if (WSACleanup() == SOCKET_ERROR) {
            print_WSA_error("Errore durante l'operazione di cleanup WSA (listener - SetConsoleCtrlHandler)");
        }
        exit(1);
    }

    int port = atoi(argv[0]);
    free(argv);

    if (get_shared_config() < 0) {
        if (WSACleanup() == SOCKET_ERROR) {
            print_WSA_error("Errore durante l'operazione di cleanup WSA (listener - get_shared_config)");
        }
        exit(1);
    }

    handle_requests(port, work_with_processes);

    if (WSACleanup() == SOCKET_ERROR) {
        print_WSA_error("Errore durante l'operazione di cleanup WSA (listener - WSACleanup)");
    }

}

BOOL WINAPI CtrlHandler2(DWORD fdwCtrlType){
    if (fdwCtrlType == CTRL_BREAK_EVENT) {
        return TRUE;
    }
}

