#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memset
#include <unistd.h>  // usleep
#include <fcntl.h>

#include "utils_v3.h"
#include "pascman.h"
#include "game.h"
#define SERVER_PORT 9501
#define KEY 19753

#define MAX_PLAYERS 2
#define BACKLOG 5
#define BUFFERSIZE 80

typedef struct Player
{
    uint32_t player_id;
    int sockfd;
} Player;

volatile sig_atomic_t end = 0;

void endServerHandler(int sig)
{
  end = 1;
}

void terminate(Player *tabPlayers, int nbPlayers)
{
  printf("\nJoueurs inscrits : \n");
  for (int i = 0; i < nbPlayers; i++)
  {
    printf("  - Joueur numéro : %u inscrit\n", tabPlayers[i].player_id);
  }
  exit(0);
}

int initSocketServer(int serverPort)
{
  int sockfd = ssocket();

  sbind(serverPort, sockfd);

  slisten(sockfd, BACKLOG);

  return sockfd;
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

int main(int argc, char** argv) {
    +
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

    // Initialisation du serveur
    int sockfd = initSocketServer(SERVER_PORT);
    printf("Le serveur tourne sur le port : %i \n", SERVER_PORT);

    int shm = sshmget(KEY, sizeof(int), IPC_CREAT | 0666); // création de la mémoire partagée
    int sem_create = sem_create(KEY, 1, IPC_CREAT | 0666, 0); // création du sémaphore
    
    struct GameState state; // chargement de la map
    FileDescriptor sout = 1;
    FileDescriptor map  = sopen("./resources/map.txt", O_RDONLY, 0);
    load_map(map, sout, &state);
    sclose(map);
    
    sshmat(shm);
    sshmdt(shm);
    
    int pipefd[2]; // pipe pour communiquer avec le broadcaster
    spipe(pipefd);
    fork_and_run1(run_broadcaster, pipefd);
    sclose(pipefd[0]); // fermeture du descripteur de lecture
    int nbChar;
    char buffer[BUFFERSIZE];
    while (!end) {
        snprintf(buffer, BUFFERSIZE,
             "J1: score=%d pos=(%d,%d) | J2: score=%d pos=(%d,%d) | food_restante=%d | terminé=%s\n",
             state.scores[0], state.positions[0].x, state.positions[0].y,
             state.scores[1], state.positions[1].x, state.positions[1].y,
             state.food_count,
             state.game_over ? "oui" : "non");

    swrite(pipefd[1], buffer, strlen(buffer));

    usleep(500000); // 500 ms d'attente
}
// boucle de lecture des infos de la map pour les écrire au broadcaster à compléter
    printf("Arrêt du serveur en cours...\n");

// Fermer le descripteur d’écriture du pipe
    sclose(pipefd[1]);

// Détacher la mémoire partagée
    sshmdt(shm);

// Supprimer la mémoire partagée
    shm_delete(KEY);

// Supprimer le sémaphore
    sem_delete(KEY);

// Afficher les joueurs connectés avant de quitter
    terminate(tabPlayers, nbPlayers);    

    while (!end) {
        
        pid_t client_handler = sfork();

        /* acceptation de la connexion */
        int newsockfd = saccept(sockfd);
        if (end)
        {
            terminate(tabPlayers, nbPlayers);
        }

        /* lecture de la connexion */
        ssize_t ret = read(newsockfd, &msg, sizeof(msg));
        if (end)
        {
            terminate(tabPlayers, nbPlayers);
        }
        checkCond(ret != sizeof(msg), "ERROR READ");

        //donne un identifiant différent à chaque joueur
        msg.registration.msgt = REGISTRATION;
        msg.registration.player = nbPlayers;

        /* note le socket du joueur à son indice*/
        tabPlayers[nbPlayers].player_id = nbPlayers;
        tabPlayers[nbPlayers].sockfd = newsockfd;
        nbPlayers++;

        printf("Inscription demandée par le joueur numéro: %u\n", msg.registration.player);
        // Afficher les joueurs inscrits à chaque inscription
        printf("Joueurs inscrits : \n");
        for (int i = 0; i < nbPlayers; i++) {
        printf("  - Joueur numéro : %u (ID du joueur: %d, Socket: %d)\n",
            tabPlayers[i].player_id, tabPlayers[i].player_id, tabPlayers[i].sockfd);
        }

        nwrite(newsockfd, &msg, sizeof(msg));
        printf("Nb Inscriptions : %i\n", nbPlayers);
        printf("\n");

    }
    sclose(sockfd);

    exit(0);
}
