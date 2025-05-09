#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "utils_v3.h"
#include "pascman.h"

void launch_graphical_interface() {
    sexecl("./target/release/pas-cman-ipl", "pas-cman-ipl", NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    int sockfd = ssocket();
    sconnect(server_ip, server_port, sockfd);

    // Création du pipe : direction clavier sera écrite ici par pas-cman-ipl
    int pipefd[2];
    spipe(pipefd);

    pid_t pid = sfork();
    if (pid == 0) {
        // Enfant : lance pas-cman-ipl

        // Rediriger STDIN vers sockfd (messages serveur)
        sdup2(sockfd, STDIN_FILENO);

        // Rediriger STDOUT vers pipefd[1] (direction vers client)
        sdup2(pipefd[1], STDOUT_FILENO);

        // Nettoyage
        sclose(pipefd[0]);
        sclose(pipefd[1]);
        sclose(sockfd);

        // Lancement
        launch_graphical_interface();
        exit(EXIT_FAILURE);  // sécurité
    }

    // Parent : pas_client
    sclose(pipefd[1]); // lecture uniquement

    // Inscription
    union Message msg;
    ssize_t r = sread(sockfd, &msg, sizeof(msg));
    if (r != sizeof(msg) || msg.msgt != REGISTRATION) {
        fprintf(stderr, "Erreur : échec de l'inscription.\n");
        sclose(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("✅ Connecté avec succès en tant que joueur %u !\n", msg.registration.player);


// 2. Boucle attente : on attend que le serveur envoie SPAWN (début du jeu)
bool start_game = false;
while (!start_game) {
    union Message init_msg;
    if (read(sockfd, &init_msg, sizeof(init_msg)) <= 0) break;

    if (init_msg.msgt == SPAWN) {
        start_game = true;

        // Fork et lancement de l’interface ici
        pid_t pid = sfork();
        if (pid == 0) {
            sdup2(sockfd, STDIN_FILENO);
            sdup2(pipefd[1], STDOUT_FILENO);
            sclose(pipefd[0]); sclose(pipefd[1]); sclose(sockfd);
            launch_graphical_interface();
        } else {
            sclose(pipefd[1]);
            nwrite(STDOUT_FILENO, &init_msg, sizeof(init_msg)); // envoie le premier SPAWN à l’interface
        }
    }
}

    // Boucle principale
    bool game_over = false;
    while (!game_over) {
        // Lire direction depuis pas-cman-ipl
        enum Direction dir;
        ssize_t dr = read(pipefd[0], &dir, sizeof(dir));
        if (dr == sizeof(dir)) {
            swrite(sockfd, &dir, sizeof(dir));
        }

        // Lire message serveur
        union Message msg_in;
        ssize_t sr = read(sockfd, &msg_in, sizeof(msg_in));
        if (sr == sizeof(msg_in)) {
            nwrite(STDOUT_FILENO, &msg_in, sizeof(msg_in));  // transmis à pas-cman-ipl
            if (msg_in.msgt == GAME_OVER) {
                game_over = true;
            }
        }

        usleep(5000);
    }

    sclose(pipefd[0]);
    sclose(sockfd);
    return EXIT_SUCCESS;
}
