#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // memset
#include <unistd.h>  // usleep
#include <fcntl.h>

#include "utils_v3.h"
#include "pascman.h"
#include "game.h"
#define PORT 9501
#define KEY 19753

#define BUFFERSIZE 80

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
    while ((nbChar = sread(0,))) // boucle de lecture des infos de la map pour les écrire au broadcaster à compléter

    
    while (1) {
        pid_t client_handler = sfork();

        

    }

    exit(0);
}