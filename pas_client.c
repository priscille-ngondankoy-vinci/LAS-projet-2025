#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "utils_v3.h"
#include "pascman.h"

// Fonction pour lancer l'interface graphique pas-cman-ipl
void launch_graphical_interface(int pipe_read_end) {
    // Redirige l'entrée standard vers le pipe (lecture depuis pas_client)
    sdup2(pipe_read_end, STDIN_FILENO);

    // Lancement de pas-cman-ipl (il lira depuis stdin, et écrira les directions sur stdout)
    sexecl("./target/release/pas-cman-ipl", "pas-cman-ipl", NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    // 1. Connexion au serveur via socket
    int sockfd = ssocket();
    sconnect(server_ip, server_port, sockfd);

    // 2. Attente de la confirmation d'inscription
    union Message msg;
    sread(sockfd, &msg, sizeof(msg));
    if (msg.msgt != REGISTRATION) {
        fprintf(stderr, "Erreur : échec de l'inscription au serveur.\n");
        sclose(sockfd);
        exit(EXIT_FAILURE);
    }

    // 3. Création d’un seul pipe :
    //    - lecture côté pas_client
    //    - écriture côté pas-cman-ipl
    int pipefd[2];
    spipe(pipefd);

    pid_t pid = sfork();
    if (pid == 0) {
        // Enfant : lance pas-cman-ipl

        sclose(pipefd[0]); // On ne lit pas ici

        // Redirections : stdout vers pipe (pour envoyer les directions)
        sdup2(pipefd[1], STDOUT_FILENO);

        // stdin sera redirigé par la fonction launch_graphical_interface()
        launch_graphical_interface(pipefd[0]);

        // Ne devrait jamais arriver ici
        exit(EXIT_FAILURE);
    }

    // Parent (pas_client)
    sclose(pipefd[1]); // Pas besoin d'écriture dans le pipe ici

    bool game_over = false;

    while (!game_over) {
        // 1. Lecture d'une direction depuis pas-cman-ipl (écrite sur stdout → pipefd[0])
        enum Direction dir;
        ssize_t dr = read(pipefd[0], &dir, sizeof(dir));
        if (dr == sizeof(dir)) {
            // Transmet la direction au serveur via socket
            swrite(sockfd, &dir, sizeof(dir));
        }

        // 2. Lecture des messages du serveur
        union Message msg_in;
        ssize_t sr = read(sockfd, &msg_in, sizeof(msg_in));
        if (sr == sizeof(msg_in)) {
            // Envoie ce message à pas-cman-ipl (lu sur stdin)
            nwrite(STDOUT_FILENO, &msg_in, sizeof(msg_in));

            // Vérifie si la partie est terminée
            if (msg_in.msgt == GAME_OVER) {
                game_over = true;
            }
        }

        // Pause optionnelle pour éviter une boucle trop rapide
        usleep(5000);
    }

    // Nettoyage
    sclose(pipefd[0]);
    sclose(sockfd);

    return EXIT_SUCCESS;
}
