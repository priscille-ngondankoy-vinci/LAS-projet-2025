#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "utils_v3.h"
#include "pascman.h"
#include "game.h"

#define SERVER_PORT 9090
#define SERVER_IP "127.0.0.1"
#define KEY 19753
#define MAX_PSEUDO 256
#include <termios.h>
#include <unistd.h>

// Active le mode raw (pas de buffer, pas d’echo)
void enable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Remet le terminal en mode normal
void disable_raw_mode() {
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}


int initSocketClient(char * serverIP, int serverPort)
{
  int sockfd = ssocket();
  sconnect(serverIP, serverPort, sockfd);
  return sockfd;
}

void run_pas_cman_ipl(void *arg_sockfd, void *arg_pipe_write) {
  int sockfd = *(int *)arg_sockfd;
  int pipe_write = *(int *)arg_pipe_write;

  // Rediriger stdin vers la socket (pour que pas-cman-ipl lise les messages depuis le serveur)
  sdup2(sockfd, 0);         
  // Rediriger stdout vers le pipe (pour que pas-cman-ipl écrive ses messages dans le pipe)
  sdup2(pipe_write, 1);  

  close(sockfd);
  close(pipe_write);

  sexecl("./target/release/pas-cman-ipl", "pas-cman-ipl", NULL);
}

int main(int argc, char **argv){
  uint32_t player_id;
  char pseudo[MAX_PSEUDO];
  union Message msg;
  int sockfd = initSocketClient(SERVER_IP, SERVER_PORT);

  // Création du pipe : écriture par pas-cman-ipl (stdout), lecture par pas-client
  int pipe[2];
  spipe(pipe);

  int psockfd = sockfd;
  int ppipe_write = pipe[1];

  fork_and_run2(run_pas_cman_ipl, &psockfd, &ppipe_write);

  close(pipe[1]); 

  /* retrieve player name */
  printf("Bienvenue dans le programme d'inscription au serveur de jeu\n");
  printf("Entrez votre pseudo :\n");
  int ret = sread(0, pseudo, MAX_PSEUDO);
  if (ret < 2) {
    printf("Erreur: vous devez entrer un nom de joueur\n");
    exit(1);
  }
  pseudo[ret - 1] = '\0';

  // Envoi du message d'enregistrement au serveur
  msg.registration.msgt = REGISTRATION;
  swrite(sockfd, &msg, sizeof(msg));
  sread(sockfd, &msg, sizeof(msg));
  player_id = msg.registration.player;
  // Affichage du numéro de joueur
  printf("Je suis le joueur numéro %u\n", msg.registration.player);

 while (1) {
    enum Direction dir;
    ssize_t ret = sread(pipe[0], &dir, sizeof(dir));
    if (ret <= 0) break;

    // Construction correcte du message
    msg.msgt = MOVEMENT;
    msg.movement.msgt = MOVEMENT;
    msg.movement.id = player_id;

    // La position n’est pas une position relative ici : on envoie uniquement la direction
    // Elle sera interprétée côté serveur
    msg.movement.pos.x = dir; // ⚠️ Utilisé juste comme transport du code direction
    msg.movement.pos.y = 0;

    swrite(sockfd, &msg, sizeof(msg));
}



  disable_raw_mode();


  sclose(sockfd);
  close(pipe[0]);

  return 0;
}

