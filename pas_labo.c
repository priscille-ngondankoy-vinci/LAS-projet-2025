#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "utils_v3.h"
#define SERVER_PORT 9090

#define SERVER_EXEC "./pas_server"
#define CLIENT_EXEC "./pas_client"


int main() {

  pid_t server_pid, client1_pid, client2_pid;
  char port_str[10];
  sprintf(port_str, "%d", SERVER_PORT);

  // Démarrer le serveur
  server_pid = fork();
    if (server_pid == 0) {
        sexecl(SERVER_EXEC, SERVER_EXEC, port_str, NULL);
        perror("Erreur lancement serveur");
        exit(1);
    }

    sleep(10);


    // lancement des 2 clients 

    client1_pid = fork();
    if (client1_pid == 0) {
        sexecl(CLIENT_EXEC, CLIENT_EXEC, "127.0.0.1", port_str, NULL);
        perror("Erreur lancement client 1");
        exit(1);
    }

    sleep(10);

    client2_pid = fork();
    if (client2_pid == 0) {
        sexecl(CLIENT_EXEC, CLIENT_EXEC, "127.0.0.1", port_str, NULL);
        perror("Erreur lancement client 2");
        exit(1);
    }
    waitpid(client1_pid, NULL, 0);
    waitpid(client2_pid, NULL, 0);

    kill(server_pid, SIGINT);

    waitpid(server_pid, NULL, 0);

    printf("Tous les processus sont terminés\n");
    return 0;
}
