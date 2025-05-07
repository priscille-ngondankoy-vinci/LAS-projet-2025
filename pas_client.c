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
#define SERVER_PORT 9501
#define SERVER_IP "127.0.0.1"
#define KEY 19753
#define MAX_PSEUDO 256

int initSocketClient(char * serverIP, int serverPort)
{
  int sockfd = ssocket();
  sconnect(serverIP, serverPort, sockfd);
  return sockfd;
}

int main(int argc, char **argv){
  char pseudo[MAX_PSEUDO];
  union Message msg;

  //création du pipe : écriture de pas-client et lecture de pas-cman-ipl
  //création du pipe : écriture de pas-cman-ipl et lecture de pas-client
  int pipe1[2], pipe2[2];
  spipe(pipe1);
  spipe(pipe2);

  //processus fils pas-cman-ipl
  pid_t pid = sfork();
  if(pid == 0){

    sdup2(pipe1[0], 0); // redirige l'entrée standard vers (une copie) du pipe de pas-client
    sdup2(pipe2[1], 1); //redirige la sortie standard vers le pipe de pas-client
    //ferme les descripteurs de pipe inutilisés puisque redirigés vers stdin et stdout
    close(pipe1[0]); //fermé car en double
    close(pipe1[1]); //inutilisé
    close(pipe2[0]); //inutilisé
    close(pipe2[1]); //fermé car en double
    
    //remplace le processus courant par pascman-ipl
    sexecl("./target/release/pas-cman-ipl","pas-cman-ipl")
  }

  close(pipe1[0]);
  close(pipe2[1]);

  /* retrieve player name */
  printf("Bienvenue dans le programe d'inscription au serveur de jeu\n");
  printf("Entrez votre pseudo :\n");
  int ret = sread(0, pseudo, MAX_PSEUDO);
  if (ret < 2) {
        printf("Erreur: vous devez entrer un nom de joueur\n");
        exit(1);
  }
  pseudo[ret - 1] = '\0';
  msg.registration.msgt = REGISTRATION;
  swrite(pipe1[1], &msg, sizeof(msg));

  int sockfd = initSocketClient(SERVER_IP, SERVER_PORT);
  swrite(sockfd, &msg, sizeof(msg));

  /* wait server response */
  sread(sockfd, &msg, sizeof(msg));
  printf("Je suis le joueur numéro %u\n", msg.registration.player);

  sclose(sockfd);
  return 0;
}

