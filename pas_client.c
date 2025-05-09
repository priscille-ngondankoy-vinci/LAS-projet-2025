#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "utils_v3.h"
#include "pascman.h"

void launch_graphical_interface(int pipe_read_end) {
    // Redirige l'entrée standard vers le pipe (pas_client → pas-cman-ipl)
    sdup2(pipe_read_end, STDIN_FILENO);
    sexecl("./target/release/pas-cman-ipl", "pas-cman-ipl", NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    // Connexion au serveur
    int sockfd = ssocket();
    sconnect(server_ip, server_port, sockfd);

    // Inscription (réception du message REGISTRATION)
    union Message msg;
    sread(sockfd, &msg, sizeof(msg));
    if (msg.msgt != REGISTRATION) {
        fprintf(stderr, "Erreur : échec de l'inscription au serveur.\n");
        sclose(sockfd);
        exit(EXIT_FAILURE);
    }

    // Pipe pour lire les directions envoyées par pas-cman-ipl
    int pipe_keyboard[2]; // [1] sera stdout de pas-cman-ipl, [0] lu par pas_client
    spipe(pipe_keyboard);

    // Pipe pour écrire les messages serveur vers pas-cman-ipl
    int pipe_to_ipl[2];   // [1] utilisé par pas_client, [0] devient stdin de pas-cman-ipl
    spipe(pipe_to_ipl);

    pid_t pid = sfork();
    if (pid == 0) {
        // Processus enfant : pas-cman-ipl
        sclose(pipe_keyboard[0]); // inutile ici
        sclose(pipe_to_ipl[1]);   // inutile ici

        // Redirections
        sdup2(pipe_keyboard[1], STDOUT_FILENO);
        sdup2(pipe_to_ipl[0], STDIN_FILENO);

        // Lancer l'interface graphique
        launch_graphical_interface(pipe_to_ipl[0]);
        exit(EXIT_FAILURE); // sécurité
    }

    // Processus parent : pas_client
    sclose(pipe_keyboard[1]); // inutiles
    sclose(pipe_to_ipl[0]);

    bool game_over = false;

    while (!game_over) {
        // 1. Lire les directions du joueur depuis pas-cman-ipl
        enum Direction dir;
        ssize_t dr = read(pipe_keyboard[0], &dir, sizeof(dir));
        if (dr == sizeof(dir)) {
            swrite(sockfd, &dir, sizeof(dir));
        }

        // 2. Lire les messages du serveur
        union Message msg_in;
        ssize_t sr = read(sockfd, &msg_in, sizeof(msg_in));
        if (sr == sizeof(msg_in)) {
            nwrite(pipe_to_ipl[1], &msg_in, sizeof(msg_in));

            if (msg_in.msgt == GAME_OVER) {
                game_over = true;
            }
        }

        // Petite pause pour éviter de tourner trop vite (optionnel)
        usleep(5000);
    }

    // Nettoyage
    sclose(pipe_keyboard[0]);
    sclose(pipe_to_ipl[1]);
    sclose(sockfd);

    return EXIT_SUCCESS;
}
