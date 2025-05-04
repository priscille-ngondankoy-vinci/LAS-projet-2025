#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define SERVER_EXEC "./pas_server"
#define CLIENT_EXEC "./pas_client"


int main() {

  pid_t server_pid, client1_pid, client2_pid;

  // Démarrer le serveur
  server_pid = fork();
    if (server_pid == 0) {
        execl(SERVER_EXEC, SERVER_EXEC, "9501", NULL);
        perror("Erreur lancement serveur");
        exit(1);
    }

    sleep(10);


    // lancement des 2 clients 

    client1_pid = fork();
    if (client1_pid == 0) {
        execl(CLIENT_EXEC, CLIENT_EXEC, "127.0.0.1", "9501", NULL);
        perror("Erreur lancement client 1");
        exit(1);
    }

    sleep(10);

    client2_pid = fork();
    if (client2_pid == 0) {
        execl(CLIENT_EXEC, CLIENT_EXEC, "127.0.0.1", "9501", NULL);
        perror("Erreur lancement client 2");
        exit(1);
    }
    waitpid(client1_pid, NULL, 0);
    waitpid(client2_pid, NULL, 0);


}