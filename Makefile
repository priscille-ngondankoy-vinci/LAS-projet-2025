
CC=gcc

CFLAGS=-std=c17 -pedantic -Wall -Wvla -Werror  -Wno-unused-variable -Wno-unused-but-set-variable -D_DEFAULT_SOURCE

all: exemple pas_server pas_client pas_labo

exemple: exemple.o game.o utils_v3.o
	$(CC) $(CFLAGS) -o exemple exemple.o game.o utils_v3.o

exemple.o: exemple.c
	$(CC) $(CFLAGS) -c exemple.c

pas_labo: pas_labo.o game.o utils_v3.o
	$(CC) $(CFLAGS) -o pas_labo pas_labo.o game.o utils_v3.o

pas_labo.o: pas_labo.c game.h utils_v3.h pascman.h
	$(CC) $(CFLAGS) -c pas_labo.c

pas_server: pas_server.o game.o utils_v3.o
	$(CC) $(CFLAGS) -o pas_server pas_server.o game.o utils_v3.o

pas_server.o: pas_server.c game.h utils_v3.h pascman.h
	$(CC) $(CFLAGS) -c pas_server.c

pas_client: pas_client.o game.o utils_v3.o
	$(CC) $(CFLAGS) -o pas_client pas_client.o game.o utils_v3.o

pas_client.o: pas_client.c game.h utils_v3.h pascman.h
	$(CC) $(CFLAGS) -c pas_client.c

clean:
	rm -f *.o exemple pas_server pas_client pas_labo


