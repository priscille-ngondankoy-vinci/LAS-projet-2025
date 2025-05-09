#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#include "utils_v3.h"
#include "pascman.h"
#include "game.h"

#define MAX_PLAYERS 2
#define BACKLOG 5
#define SHM_KEY 1234
#define SEM_KEY 5678

volatile sig_atomic_t server_running = 1;
volatile sig_atomic_t timeout_occurred = 0;

void handle_sigint(int sig) {
    server_running = 0;
}

void handle_sigalrm(int sig) {
    timeout_occurred = 1;
}

int initSocketServer(int serverPort) {
  int sockfd = ssocket();

  int option = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

  sbind(serverPort, sockfd);
  slisten(sockfd, BACKLOG);
  return sockfd;
}


void run_broadcaster(int read_fd, int socket1, int socket2, int player_count) {
    union Message msg;
    int sockets[MAX_PLAYERS] = { socket1, socket2 };

    while (1) {
        ssize_t r = sread(read_fd, &msg, sizeof(msg));
        if (r == 0) break;
        for (int i = 0; i < player_count; i++) {
            swrite(sockets[i], &msg, sizeof(msg));
        }
    }
}

void run_client_handler(int client_fd, int player_id, struct GameState *state, int sem_id, int pipe_bcast_write_fd) {
    union Message msg;
    while (sread(client_fd, &msg, sizeof(msg)) > 0) {
        if (msg.msgt == MOVEMENT) {
            sem_down0(sem_id);
            bool game_over = process_user_command(state,
                player_id == 1 ? PLAYER1 : PLAYER2,
                (enum Direction) msg.movement.pos.x,
                pipe_bcast_write_fd);
            sem_up0(sem_id);
            if (game_over) break;
        }
    }
    sclose(client_fd);
    exit(0);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    ssigaction(SIGINT, handle_sigint);
    ssigaction(SIGALRM, handle_sigalrm);

    int sockfd = initSocketServer(port);
    printf("Serveur lancé sur le port %d\n", port);

    int shm_id = sshmget(SHM_KEY, sizeof(struct GameState), IPC_CREAT | 0666);
    struct GameState *state = sshmat(shm_id);
    int sem_id = sem_create(SEM_KEY, 1, 0666, 1);

    int pipe_bcast[2];
    spipe(pipe_bcast);

    int client_sockets[MAX_PLAYERS];
    pid_t child_pids[MAX_PLAYERS];
    int player_count = 0;

    while (server_running) {
        if (player_count == 0) {
            // Attente du premier joueur
            int client_fd = saccept(sockfd);
            union Message reg_msg;
            ssize_t r = sread(client_fd, &reg_msg, sizeof(reg_msg));
            if (r <= 0 || reg_msg.msgt != REGISTRATION) {
                sclose(client_fd);
                continue;
            }

            send_registered(1, client_fd);
            client_sockets[0] = client_fd;
            child_pids[0] = -1;
            player_count = 1;
            alarm(30);
            printf(" Joueur %d connecté.\n", player_count );
            continue;
        }

        if (player_count == 1) {
            if (timeout_occurred) {
                printf("Timeout : joueur 2 absent, déconnexion du joueur 1.\n");
                sclose(client_sockets[0]);
                timeout_occurred = 0;
                player_count = 0;
                continue;
            }

            int client_fd = saccept(sockfd);
            alarm(0);  // Annule le timer

            union Message reg_msg;
            ssize_t r = sread(client_fd, &reg_msg, sizeof(reg_msg));
            if (r <= 0 || reg_msg.msgt != REGISTRATION) {
                sclose(client_fd);
                continue;
            }

            send_registered(2, client_fd);
            client_sockets[1] = client_fd;

            printf(" Joueur %d connecté avec succès.\n", player_count + 1);

            pid_t pid1 = sfork();
            if (pid1 == 0) {
                sclose(sockfd); sclose(pipe_bcast[0]);
                run_client_handler(client_sockets[0], 1, state, sem_id, pipe_bcast[1]);
            }
            child_pids[0] = pid1;

            pid_t pid2 = sfork();
            if (pid2 == 0) {
                sclose(sockfd); sclose(pipe_bcast[0]);
                run_client_handler(client_sockets[1], 2, state, sem_id, pipe_bcast[1]);
            }
            child_pids[1] = pid2;

            player_count = 2;
            break;
        }
    }

    if (player_count == 2) {
        int fdmap = sopen("./resources/map.txt", O_RDONLY, 0);
        load_map(fdmap, pipe_bcast[1], state);
        sclose(fdmap);

        pid_t bcast_pid = sfork();
        if (bcast_pid == 0) {
            run_broadcaster(pipe_bcast[0], client_sockets[0], client_sockets[1], player_count);
            exit(0);
        }

        for (int i = 0; i < player_count; i++) {
            swaitpid(child_pids[i], NULL, 0);
        }

        skill(bcast_pid, SIGTERM);
        swaitpid(bcast_pid, NULL, 0);
    }

    sshmdt(state);
    sshmdelete(shm_id);
    sem_delete(sem_id);
    sclose(sockfd);
    sclose(pipe_bcast[0]);
    sclose(pipe_bcast[1]);

    printf("Serveur terminé proprement.\n");
    return 0;
}
