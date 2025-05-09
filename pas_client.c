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

int initSocketClient(char * serverIP, int serverPort)
{
  int sockfd = ssocket();
  sconnect(serverIP, serverPort, sockfd);
  return sockfd;
}

void run_pas_cman_ipl(void *arg_sockfd, void *arg_pipe_write) {
  int sockfd = *(int *)arg_sockfd;
  int pipe_write = *(int *)arg_pipe_write;

  sdup2(sockfd, 0);         
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

  int pipe[2];
  spipe(pipe);

  int psockfd = sockfd;
  int ppipe_write = pipe[1];

  fork_and_run2(run_pas_cman_ipl, &psockfd, &ppipe_write);

  close(pipe[1]); 

  printf("Bienvenue dans le programme d'inscription au serveur de jeu\n");
  printf("Entrez votre pseudo :\n");
  int ret = sread(0, pseudo, MAX_PSEUDO);
  if (ret < 2) {
    printf("Erreur: vous devez entrer un nom de joueur\n");
    exit(1);
  }
  pseudo[ret - 1] = '\0';

  msg.registration.msgt = REGISTRATION;
  swrite(sockfd, &msg, sizeof(msg));
  sread(sockfd, &msg, sizeof(msg));
  player_id = msg.registration.player;

 while (1) {
    enum Direction dir;
    ssize_t ret = sread(pipe[0], &dir, sizeof(dir));
    if (ret <= 0) break;

    msg.msgt = MOVEMENT;
    msg.movement.msgt = MOVEMENT;
    msg.movement.id = player_id;


    msg.movement.pos.x = dir;
    msg.movement.pos.y = 0;

    swrite(sockfd, &msg, sizeof(msg));
}

  sclose(sockfd);
  close(pipe[0]);

  return 0;
}

