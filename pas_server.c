#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memset
#include <unistd.h>  // usleep
#include <fcntl.h>
#include <sys/wait.h> // pour waitpid
#include <signal.h>

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
                else continue; // ignore si direction invalide

                sem_down(sem_id, 0);
                bool game_over = process_user_command(state, player_id == 0 ? PLAYER1 : PLAYER2, dir, client_sockfd);
                sem_up(sem_id, 0);

                if (game_over) {
                    end = 1; // marquer fin du jeu pour sortir de la boucle
                }

                break;
            }

            case REGISTRATION:
                printf("Joueur %d s’est déjà enregistré.\n", player_id);
                break;

            default:
                printf("Message inconnu reçu de joueur %d\n", player_id);
                break;
        }
    }

    // Préparer le message de fin de partie
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
    printf("Le serveur tourne sur le port : %i \n", SERVER_PORT);

    int shm = sshmget(KEY, sizeof(struct GameState), IPC_CREAT | 0666);
    struct GameState *state = (struct GameState *)sshmat(shm);

    int sem_id = sem_create(KEY, 1, IPC_CREAT | 0666, 0);

    // Initialiser la map (diffusion sur pipefd[1] plus tard)
    FileDescriptor sout = 1;
    FileDescriptor map = sopen("./resources/map.txt", O_RDONLY, 0);
    load_map(map, sout, state);
    sclose(map);

    int pipefd[2];
    spipe(pipefd);
    fork_and_run1(run_broadcaster, pipefd);
    sclose(pipefd[0]);  // fermer lecture côté parent

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
                    // Passer pipefd[1] comme "canal de diffusion" (pas juste le client)
                    client_handler_func(nbPlayers, newsockfd, state, sem_id, pipefd[1]);
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

  sbind(serverPort, sockfd);

  slisten(sockfd, BACKLOG);

  return sockfd;
}



