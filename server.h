#ifndef _SERVER_H
#define _SERVER_H

#include "message.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_CLIENTS 3
#define MAX_SESSIONS 10

struct user{
    pthread_t thread;
    char clientID[MAX_NAME];
    struct sockaddr_storage clientaddr;
    socklen_t size;
    char sessID[MAX_DATA];
    int sockfd;
    int logged;
};

char usernames[MAX_CLIENTS][MAX_NAME+1] = {"Shrenik", "Test", "User123"};
char passwords[MAX_CLIENTS][MAX_NAME+1] = {"12345", "asdf", "qwerty"};

struct user* clients[MAX_CLIENTS];
char* sessions[MAX_SESSIONS];

int check_available_client();
int check_available_session();
int check_existing_session();

int check_user_online(char* user, char* curr);

int verify_user(struct message* msg);
int logged_user(struct message* msg);

int add_client(struct user* usr);
int remove_client(struct user* usr);

int add_session(char* sessID);
int remove_session();

int setup(char* port);

void* handle_client(void* usr);

#endif