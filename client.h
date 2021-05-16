#ifndef _CLIENT_H
#define _CLIENT_H

#include "message.h"

struct state{
	char* clientID;
	char* sessID;
	int invited;
	char* inv_sessID;
};

int login(int sockfd, const unsigned char clientID[MAX_NAME], const unsigned char pass[MAX_DATA]);
int logout(int sockfd, const unsigned char clientID[MAX_NAME]);
int join_session(int sockfd, const unsigned char sessID[MAX_DATA]);
int leave_session(int sockfd);
int create_session(int sockfd, const unsigned char sessID[MAX_DATA]);
int list(int sockfd);
int send_message(int sockfd, const char* data);
void* receive(void* P_sockfd);

// Extra features
int whisper(int sockfd, const unsigned char* data);
int invite_user(int sockfd, const unsigned char* data);

#endif
