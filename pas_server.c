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

int main(int argc, char** argv) {
    struct GameState state;
    FileDescriptor sout = 1;
    FileDescriptor map  = sopen("./resources/map.txt", O_RDONLY, 0);
    load_map(map, sout, &state);
    sclose(map);
    pid_t broadcaster = sfork();
    
    while (1) {
        pid_t client_handler = sfork();
        

    }

    exit(0);
}