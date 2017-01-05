CC = gcc
CFLAGS = -ggdb3 -std=c99 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200112L

EXEs = battle_client battle_server

all: $(EXEs)
.PHONY: all clean

battle_client: battle_client.c
	$(CC) $(CFLAGS) -o battle_client battle_client.c

battle_server: battle_server.c
	$(CC) $(CFLAGS) -o battle_server battle_server.c

clean:
	- rm $(EXEs)
