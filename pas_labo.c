#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "utils_v3.h"

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <port> <map_file> <joueur1_file> <joueur2_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *port = argv[1];
    char *map_file = argv[2];
    char *j1_file = argv[3];
    char *j2_file = argv[4];

    int pipe1[2], pipe2[2];
    spipe(pipe1);
    spipe(pipe2);

    if (sfork() == 0)
        sexecl("./pas_server", "pas_server", port, map_file, NULL);

    usleep(800000);

    if (sfork() == 0) {
        sdup2(pipe1[0], STDIN_FILENO);
        sclose(pipe1[1]);
        sexecl("./pas_client", "pas_client", "127.0.0.1", port, "-test", NULL);
    }
    sclose(pipe1[0]);

    usleep(300000);

    if (sfork() == 0) {
        sdup2(pipe2[0], STDIN_FILENO);
        sclose(pipe2[1]);
        sexecl("./pas_client", "pas_client", "127.0.0.1", port, "-test", NULL);
    }
    sclose(pipe2[0]);

    FILE *f1 = fopen(j1_file, "r");
    FILE *f2 = fopen(j2_file, "r");
    if (!f1 || !f2) {
        perror("Erreur ouverture fichiers commandes");
        sclose(pipe1[1]);
        sclose(pipe2[1]);
        exit(EXIT_FAILURE);
    }

    char c1, c2;
    while (1) {
        int r1 = fread(&c1, 1, 1, f1);
        if (r1 == 1) swrite(pipe1[1], &c1, 1);

        int r2 = fread(&c2, 1, 1, f2);
        if (r2 == 1) swrite(pipe2[1], &c2, 1);

        if (r1 != 1 && r2 != 1) break;
        usleep(100000);
    }

    usleep(3000000);

    fclose(f1);
    fclose(f2);
    sclose(pipe1[1]);
    sclose(pipe2[1]);

    return 0;
}
