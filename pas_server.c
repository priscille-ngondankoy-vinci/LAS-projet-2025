#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memset
#include <unistd.h>  // usleep
#include <fcntl.h>
#include <sys/wait.h> // pour waitpid

#include "utils_v3.h"
#include "pascman.h"
#include "game.h"

#define SERVER_PORT 9090
#define KEY 19753

#define MAX_PLAYERS 2
#define BACKLOG 5
#define BUFFERSIZE 256  // augmenté pour contenir plus de texte

typedef struct Player {
    uint32_t player_id;
    int sockfd;
    pid_t child_pid;
} Player;

volatile sig_atomic_t end = 0;

void endServerHandler(int sig) {
    end = 1;
}

void terminate(Player *tabPlayers, int nbPlayers) {
    printf("\nJoueurs inscrits : \n");
    for (int i = 0; i < nbPlayers; i++) {
        printf("  - Joueur numéro : %u inscrit\n", tabPlayers[i].player_id);
    }
    exit(0);
}

void client_handler_func(int player_id, int client_sockfd, struct GameState *state, int sem_id) {
   // handler du client
    union Message msg;
    ssigaction(SIGTERM, endServerHandler); // pour gérer la terminaison du serveur
    // tant que la partie est en cours
    while (!end && !state->game_over) {
        ssize_t ret = nread(client_sockfd, &msg, sizeof(msg));
        if (ret <= 0) {
            printf("Déconnexion ou erreur pour joueur %d\n", player_id);
            break;
        
          }
        
        // traiter le message reçu 
        switch (msg.msgt) {
            case MOVEMENT:
                printf("Joueur %d demande mouvement : dx=%d, dy=%d\n",
                       player_id, msg.movement.dx, msg.movement.dy);

                sem_down(sem_id); 
                // calculer la nouvelle position
                // vérifier si le mouvement est valide

                int new_x = state->positions[player_id].x + msg.movement.dx;
                int new_y = state->positions[player_id].y + msg.movement.dy;
                int index = new_y * LARGEUR_MAP + new_x;

                if (state->map[index] != WALL) {
                    state->positions[player_id].x = new_x;
                    state->positions[player_id].y = new_y;

                    if (state->map[index] == FOOD || state->map[index] == SUPERFOOD) {
                        state->scores[player_id] += (state->map[index] == FOOD) ? 10 : 50;
                        state->map[index] = EMPTY;
                        state->food_count--;
                    }
                }

                if (state->food_count == 0) {
                    state->game_over = true;
                }

                sem_up(sem_id);

                nwrite(client_sockfd, &msg, sizeof(msg));  // envoyer update
                break;
                  // inscription du joueur

            case REGISTRATION:
                printf("Joueur %d s’enregistre\n", player_id);
                break;

            default:
                printf("Message inconnu reçu de joueur %d\n", player_id);
                break;
        }
    }
    // fin de la partie ou déconnexion et affichage du résultat

    msg.msgt = GAME_OVER;
    if (state->scores[player_id] > state->scores[(player_id + 1) % 2]) {
        msg.game_over.result = WIN;
    } else if (state->scores[player_id] < state->scores[(player_id + 1) % 2]) {
        msg.game_over.result = LOSE;
    } else {
        msg.game_over.result = DRAW;
    }

    nwrite(client_sockfd, &msg, sizeof(msg));
    printf("Joueur %d a reçu le résultat final.\n", player_id);

    sclose(client_sockfd);
    exit(0);
}

int main(int argc, char **argv) {
    union Message msg;
    Player tabPlayers[MAX_PLAYERS];
    int nbPlayers = 0;

    sigset_t set;
    ssigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    ssigprocmask(SIG_BLOCK, &set, NULL);
    ssigaction(SIGTERM, endServerHandler);
    ssigaction(SIGINT, endServerHandler);

    int sockfd = initSocketServer(SERVER_PORT);
    printf("Le serveur tourne sur le port : %i \n", SERVER_PORT);

    int shm = sshmget(KEY, sizeof(struct GameState), IPC_CREAT | 0666);
    struct GameState *state = (struct GameState *)sshmat(shm);

    int sem_id = sem_create(KEY, 1, IPC_CREAT | 0666, 0);

    FileDescriptor sout = 1;
    FileDescriptor map = sopen("./resources/map.txt", O_RDONLY, 0);
    load_map(map, sout, state);
    sclose(map);

    int pipefd[2];
    spipe(pipefd);
    fork_and_run1(run_broadcaster, pipefd);
    sclose(pipefd[0]);

    while (!end || (end && !state->game_over)) {
        char buffer[BUFFERSIZE];
        snprintf(buffer, BUFFERSIZE,
                 "J1: score=%d pos=(%d,%d) | J2: score=%d pos=(%d,%d) | food_restante=%d | terminé=%s\n",
                 state->scores[0], state->positions[0].x, state->positions[0].y,
                 state->scores[1], state->positions[1].x, state->positions[1].y,
                 state->food_count,
                 state->game_over ? "oui" : "non");
        swrite(pipefd[1], buffer, strlen(buffer));

        int newsockfd = saccept(sockfd);
        if (newsockfd >= 0) {
            if (nbPlayers < MAX_PLAYERS) {
                pid_t child_pid = sfork();
                if (child_pid == 0) {
                    client_handler_func(nbPlayers, newsockfd, state, sem_id);
                    exit(0);
                }

                msg.registration.msgt = REGISTRATION;
                msg.registration.player = nbPlayers;
                tabPlayers[nbPlayers].player_id = nbPlayers;
                tabPlayers[nbPlayers].sockfd = newsockfd;
                tabPlayers[nbPlayers].child_pid = child_pid;
                nbPlayers++;

                printf("Inscription joueur numéro: %u\n", msg.registration.player);
                nwrite(newsockfd, &msg, sizeof(msg));
            } else {
                printf("Nombre maximum de joueurs atteint, refus de connexion.\n");
                sclose(newsockfd);
            }
        }

        while (waitpid(-1, NULL, WNOHANG) > 0) {
            // nettoyer les processus zombies
        }

        usleep(500000);
    }

    printf("Arrêt du serveur en cours...\n");

    for (int i = 0; i < nbPlayers; i++) {
        kill(tabPlayers[i].child_pid, SIGTERM);
        waitpid(tabPlayers[i].child_pid, NULL, 0);
        sclose(tabPlayers[i].sockfd);
    }

    sclose(pipefd[1]);
    sshmdt(state);
    shm_delete(KEY);
    sem_delete(KEY);
    sclose(sockfd);

    terminate(tabPlayers, nbPlayers);
    exit(0);
}


