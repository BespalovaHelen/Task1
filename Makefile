CC = gcc
CFLAGS = -Wall -Wextra -O2

all: server client

server: server.c protocol.h
	$(CC) $(CFLAGS) -o server server.c

client: client.c protocol.h
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client

.PHONY: all clean run-server run-client
