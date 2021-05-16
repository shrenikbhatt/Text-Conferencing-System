CC = gcc
CFLAGS = -pthread


all: 
	$(CC) $(CFLAGS) client.c message.c -o client
	$(CC) $(CFLAGS) server.c message.c -o server	
clean: 
	rm -f client
	rm -f server
