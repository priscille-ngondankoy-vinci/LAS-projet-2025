#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memset
#include <unistd.h>  // usleep
#include <fcntl.h>
#include <sys/wait.h> // pour waitpid
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "utils_v3.h"
#include "pascman.h"
#include "game.h"
#include <stdint.h>


#define SERVER_PORT 9090
#define KEY 19753

#define MAX_PLAYERS 2
#define BACKLOG 5
#define BUFFERSIZE 256

// Ajout des définitions manquantes
#define LARGEUR_MAP 20
#define EMPTY 0
#define WIN 1
#define LOSE 2
#define DRAW 3

typedef struct Player {
    uint32_t player_id;
    int sockfd;
    pid_t child_pid;
} Player;
int initSocketServer(int serverPort);
void run_broadcaster(void *argv);
void client_handler_func(int player_id, int client_sockfd, struct GameState *state, int sem_id);

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
    union Message msg;
    ssigaction(SIGTERM, endServerHandler);

    // Enregistrer le joueur proprement
    send_registered(player_id, client_sockfd);

    // Charger la map pour ce client
    FileDescriptor map = sopen("./resources/map.txt", O_RDONLY, 0);
    if (map < 0) {
        perror("Erreur ouverture map.txt");
        exit(EXIT_FAILURE);
    }
    load_map(map, client_sockfd, state);
    sclose(map);

    while (!end && !state->game_over) {
        ssize_t ret = sread(client_sockfd, &msg, sizeof(msg));
        if (ret <= 0) {
            printf("Déconnexion ou erreur pour joueur %d\n", player_id);
            break;
        }

        switch (msg.msgt) {
            case MOVEMENT: {
                printf("Joueur %d demande mouvement : dx=%d, dy=%d\n",
                       player_id, msg.movement.pos.x, msg.movement.pos.y);

                enum Direction dir;
                if (msg.movement.pos.x == 1) dir = RIGHT;
                else if (msg.movement.pos.x == -1) dir = LEFT;
                else if (msg.movement.pos.y == 1) dir = DOWN;
                else if (msg.movement.pos.y == -1) dir = UP;
                else continue; // ignorer si direction invalide

                sem_down(sem_id, 0);
                process_user_command(state, player_id == 0 ? PLAYER1 : PLAYER2, dir, client_sockfd);
                sem_up(sem_id, 0);

                break;
            }

            case REGISTRATION:
                printf("Joueur %d s’est déjà enregistré.\n", player_id);
                send_registered(player_id, client_sockfd);
                break;

            default:
                printf("Message inconnu reçu de joueur %d\n", player_id);
                break;
        }
    }

    // Préparer et envoyer le message de fin de partie
    msg.msgt = GAME_OVER;
    if (state->scores[player_id] > state->scores[(player_id + 1) % 2]) {
        msg.game_over.winner = WIN;
    } else if (state->scores[player_id] < state->scores[(player_id + 1) % 2]) {
        msg.game_over.winner = LOSE;
    } else {
        msg.game_over.winner = DRAW;
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
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    printf("Le serveur tourne sur le port : %i \n", SERVER_PORT);

    int shm = sshmget(KEY, sizeof(struct GameState), IPC_CREAT | 0666);
    struct GameState *state = (struct GameState *)sshmat(shm);

    int sem_id = sem_create(KEY, 1, IPC_CREAT | 0666, 0);

    while (!end || (end && !state->game_over)) {
        int newsockfd = saccept(sockfd);
        if (newsockfd >= 0) {
            if (nbPlayers < MAX_PLAYERS) {
                pid_t child_pid = sfork();
                if (child_pid == 0) {
                    client_handler_func(nbPlayers, newsockfd, state, sem_id);
                    exit(0);
                }

                tabPlayers[nbPlayers].player_id = nbPlayers;
                tabPlayers[nbPlayers].sockfd = newsockfd;
                tabPlayers[nbPlayers].child_pid = child_pid;
                nbPlayers++;

                printf("Inscription joueur numéro: %u\n", nbPlayers - 1);
            } else {
                printf("Nombre maximum de joueurs atteint, refus de connexion.\n");
                sclose(newsockfd);
            }
        }

        while (waitpid(-1, NULL, WNOHANG) > 0) {}

        usleep(500000);
    }

    printf("Arrêt du serveur en cours...\n");

    for (int i = 0; i < nbPlayers; i++) {
        kill(tabPlayers[i].child_pid, SIGTERM);
        waitpid(tabPlayers[i].child_pid, NULL, 0);
        sclose(tabPlayers[i].sockfd);
    }

    sshmdt(state);
    sshmdelete(KEY);
    sem_delete(KEY);
    sclose(sockfd);

    terminate(tabPlayers, nbPlayers);
    exit(0);
}


void run_broadcaster(void *argv)
{
   // PROCESSUS FILS
   int *pipefd = argv;
   char line[BUFFERSIZE + 1];

   // Cloture du descripteur d'écriture
   sclose(pipefd[1]);

   // Boucle de lecture sur le pipe
   // et écriture 
   int nbChar;
   while ((nbChar = sread(pipefd[0], line, BUFFERSIZE)) != 0) {
      swrite(1, line, nbChar);
   }

   // Cloture du descripteur de lecture
   sclose(pipefd[0]);
}
int initSocketServer(int serverPort)
{
  int sockfd = ssocket();
  int opt = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sbind(serverPort, sockfd);

  slisten(sockfd, BACKLOG);

  return sockfd;
}



