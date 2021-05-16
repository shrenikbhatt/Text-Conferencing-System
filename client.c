#include "client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

pthread_t receiving_thread;
struct state *info;
pthread_mutex_t info_lock = PTHREAD_MUTEX_INITIALIZER;

void* receive(void* p_sockfd){
	int sockfd = *(int*)p_sockfd;
	//free(p_sockfd);
	int bytes;
	char buf[MAX_NAME+MAX_DATA+100];

	int a = 1;

	while(a){
		if(bytes = recv(sockfd, buf, MAX_NAME+MAX_DATA+100, 0) < 0){
			printf("Error in receiving.\n");
			return NULL;
		}

		struct message* p = convert_string_to_message(buf);
		buf[0] = '\0';
		char temp[MAX_DATA];
		int i = 0;
		switch(p->type){
			case LO_ACK:
				pthread_mutex_lock(&info_lock);
				info->clientID = (char*) malloc(sizeof(char)*strlen(p->source));
				strcpy(info->clientID, p->source);
				// info->clientID = p->source;
				printf("Connected to server.\n");
				pthread_mutex_unlock(&info_lock);
				break;
			case LO_NAK:
				printf("%s\n", p->data);
				close(sockfd);
				a = 0;
				break;
			case JN_ACK:
				pthread_mutex_lock(&info_lock);
				info->sessID = (char*)malloc(sizeof(char)*strlen(p->data));
				strcpy(info->sessID, p->data);
				pthread_mutex_unlock(&info_lock);
				printf("Joined session %s successfully.\n", info->sessID);
				break;
			case JN_NAK:
				printf("%s\n", p->data);
				break;
			case NS_NAK:
				printf("%s\n", p->data);
				break;
			case NS_ACK: 
				pthread_mutex_lock(&info_lock);
				info->sessID = (char*)malloc(sizeof(char)*strlen(p->data));
				strcpy(info->sessID, p->data);
				pthread_mutex_unlock(&info_lock);
				printf("Created session %s successfully.\n", info->sessID);
				break;
			case QU_ACK:
				printf("%s\n", p->data);
				break;
			case MESSAGE:
				printf("%s: %s\n", p->source, p->data);
				break;
			case WHISP_NAK:
				printf("%s\n", p->data);
				break;
			case WHISPER:
				printf("%s<whisper>: %s\n", p->source, p->data);
				break;
			case INVITE: ;
				printf("Received invite to join session %s from %s.\n", p->data, p->source);
				printf("Type 'accept' to join or 'decline' to reject the invitation.\n");
				pthread_mutex_lock(&info_lock);
				info->invited = 1;
				info->inv_sessID = (char*)malloc(sizeof(char)*strlen(p->data));
				strcpy(info->inv_sessID, p->data);
				pthread_mutex_unlock(&info_lock);
				break;
			case INVITE_NAK:
				printf("%s\n", p->data);
				break;
			default:
				break;
		}
		free(p);
	}
	return NULL;
}

int main(int argc, char** argv){
	if (argc != 1){
		printf("No arguments are supported for this program.\n");
		return -1;
	}
	// Struct to hold session and client id
	info = (struct state*) malloc(sizeof(struct state));
	info->clientID = NULL;
	info->sessID = NULL;
	info->invited = 0;
	info->inv_sessID = NULL;	
	// For command input
	char input[100];

	int quit = 0;
	int sockfd;
	struct sockaddr_in serveraddr;

	while(!quit){

		fgets(input, 100, stdin);

		pthread_mutex_lock(&info_lock);
		if(info->invited && info->inv_sessID){
			pthread_mutex_unlock(&info_lock);
			input[strlen(input)-1] = '\0';
			while(strcmp(input, "accept") && strcmp(input, "decline")){
				fflush(stdin);
				printf("Invalid input.\n");
				printf("Type 'accept' to join or 'decline' to reject the invitation.\n");
				fgets(input, 100, stdin);
				input[strlen(input)-1] = '\0';
			}
			if (!strcmp(input, "accept")){
				// pthread_mutex_lock(&info_lock);
				if(info->sessID){
					// pthread_mutex_unlock(&info_lock);
					int fail = leave_session(sockfd);
					if(fail){
						printf("Unable to leave session.\n");
					}
					else{
						pthread_mutex_lock(&info_lock);
						printf("Left session %s successfully.\n", info->sessID);
						free(info->sessID);
						info->sessID = NULL;
						pthread_mutex_unlock(&info_lock);
					}
				}
				int fail = join_session(sockfd, info->inv_sessID);
				if(fail){
					printf("Unable to join session.\n");
				}
			}
			else{
				printf("Declined invitation.\n");
			}
			pthread_mutex_lock(&info_lock);
			free(info->inv_sessID);
			info->inv_sessID = NULL;
			pthread_mutex_unlock(&info_lock);
			continue;
		}
		pthread_mutex_unlock(&info_lock);
		if(input[0] == '/'){
			// Command parsing
			int i = 1;
			char command[20];
			
			while (input[i] != ' ' && input[i] != '\0'){
				++i;
			}
			memcpy(command, input+1, i-1);
			if(input[i] == '\0'){
				command[i-2] = '\0';
			}
			else{	
				command[i-1] = '\0';
			}
			
			if(!strcmp(command, "login")){
				pthread_mutex_lock(&info_lock);
				if(!info->clientID){
					pthread_mutex_unlock(&info_lock);
					char clientID[MAX_NAME];
					char password[MAX_DATA];
					char server_ip[20];
					char port[10];
					int curr = 0;
					++i;
					int count = i;
					while(input[i] != '\0'){
						if(input[i] == ' '){
							char temp[MAX_DATA+1];
							memcpy(temp, input+count, (i-count+1));
							temp[i-count] = '\0';
							count = i+1;
							switch(curr){
								case 0:
									strcpy(clientID, temp);
									++curr;
									break;
								case 1:
									strcpy(password, temp);
									++curr;
									break;
								case 2:
									strcpy(server_ip, temp);
									++curr;
									break;
								default:
									break;
							}
						}
						++i;
					}
					char temp[MAX_NAME+1];
					memcpy(temp, input+count, (i-count+1));
					temp[i-count-1] = '\0';
					strcpy(port, temp);

					sockfd = socket(AF_INET, SOCK_STREAM, 0);
					if(sockfd < 1){
						printf("Socket creation failure.\n");
					}
					printf("Created socket.\n");

					bzero(&serveraddr, sizeof(serveraddr));
					
					serveraddr.sin_family = AF_INET;
					serveraddr.sin_addr.s_addr = inet_addr(server_ip);
					serveraddr.sin_port = htons(atoi(port));

					if (connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0){
						printf("Could not connect to server.\n");
					}
					else{
						int fail = login(sockfd, clientID, password);
						if(fail){
							printf("Error encountered when trying to connect to server.\n");
							close(sockfd);
						}
					}	
				}
				else{
					printf("Already logged in with clientID %s.\n", info->clientID);
					pthread_mutex_unlock(&info_lock);
				}
			}
			else if(!strcmp(command, "logout")){
				if(info->clientID){
					logout(sockfd, info->clientID);
					pthread_cancel(receiving_thread);
					free(info->clientID);
					info->clientID = NULL;
					if(info->sessID)
						free(info->sessID);
					info->sessID = NULL;
					close(sockfd);
				}
				else{
					printf("Please login before attempting to logout.\n");
				}
			}
			else if(!strcmp(command, "joinsession")){
				pthread_mutex_lock(&info_lock);
				if(info->clientID){
					pthread_mutex_unlock(&info_lock);
					char sessID[MAX_DATA];
					++i;
					int count = i;
					while(input[i] != '\0'){
						++i;
					}
					memcpy(sessID, input+count, i-count-1);
					sessID[i-count-1] = '\0';
			//		printf("HERE: %s\n", sessID);
					int fail = join_session(sockfd, sessID);
					if(fail){
						printf("Unable to join session.\n");
					}
				}
				else{
					printf("Please login before attempting to join session.\n");
				}
			}
			else if(!strcmp(command, "leavesession")){
				pthread_mutex_lock(&info_lock);
				if(info->clientID && info->sessID){
					pthread_mutex_unlock(&info_lock);
					int fail = leave_session(sockfd);
					if(fail){
						printf("Unable to leave session.\n");
					}
					else{
						pthread_mutex_lock(&info_lock);
						printf("Left session %s successfully.\n", info->sessID);
						free(info->sessID);
						info->sessID = NULL;
						pthread_mutex_unlock(&info_lock);
					}	
				}
				else{
					pthread_mutex_unlock(&info_lock);
					printf("Please login and join a session before attempting to leave session.\n");
				}
			}
			else if(!strcmp(command, "createsession")){
				pthread_mutex_lock(&info_lock);
				if(info->clientID && !info->sessID){
					pthread_mutex_unlock(&info_lock);
					char sessID[MAX_DATA];
					++i;
					int count = i;
					while(input[i] != '\0'){
						++i;
					}
					memcpy(sessID, input+count, i-count-1);
					sessID[i-count-1] = '\0';
					int fail = create_session(sockfd, sessID);
					if(fail){
						printf("Unable to create session.\n");
					}
				}
				else if (info->sessID){
					pthread_mutex_unlock(&info_lock);
					printf("User already part of another session\n");
				}
				else{
					pthread_mutex_unlock(&info_lock);
					printf("Please login before attempting to create a session.\n");
				}
			}
			else if(!strcmp(command, "list")){
				if(info->clientID){
					if(list(sockfd)){
						printf("Failed to list.\n");
					}
				}
				else{
					printf("Login before you can query users and sessions.\n");
				}
			}
			else if(!strcmp(command, "quit")){
				if(info->clientID){
					logout(sockfd, info->clientID);
					pthread_cancel(receiving_thread);
					free(info->clientID);
					info->clientID = NULL;
					if(info->sessID)
						free(info->sessID);
					info->sessID = NULL;
					close(sockfd);
				}
				break;
			}
			else if (!strcmp(command, "whisper")){
				pthread_mutex_lock(&info_lock);
				if(info->clientID){
					pthread_mutex_unlock(&info_lock);
					char data[MAX_DATA];
					++i;
					int count = i;
					while(input[i] != '\0'){
						++i;
					}
					memcpy(data, input+count, i-count-1);
					data[i-count-1] = '\0';

					int fail = whisper(sockfd, data);
					if(fail){
						printf("Unable to whisper message");
					}
				}
				else{
					pthread_mutex_unlock(&info_lock);
					printf("Please login before whispering to another user.\n");
				}
			}
			else if (!strcmp(command, "invite")){
				pthread_mutex_lock(&info_lock);
				if(info->clientID && info->sessID){
					pthread_mutex_unlock(&info_lock);
					char invID[MAX_DATA];
					++i;
					int count = i;
					while(input[i] != '\0'){
						++i;
					}
					memcpy(invID, input+count, i-count-1);
					invID[i-count-1] = '\0';
					int fail = invite_user(sockfd, invID);
					if(fail){
						printf("Unable to invite user.\n");
					}
				}
				else{
					pthread_mutex_unlock(&info_lock);
					printf("Must be logged in and part of a session to invite someone.\n");
				}
			}
			else{
				printf("%s is an invaild command. Please try again.\n", command);
				fflush(stdin);
			}
		}
		else if(info->clientID && info->sessID){
			// Connected to a session already (implies login successful)
			int i =0;
			while(input[i] != '\0'){
				++i;
			}
		//	printf("%d\n", i);
			input[i-1] = '\0';
			int fail = send_message(sockfd, input);
			if(fail){
				printf("Unable to send data.\n");	
			}
		}
		else{
			printf("Please login and join session before attempting to send data.\n");
		}
	}
	return 0;
}


int login(int sockfd, const unsigned char clientID[MAX_NAME], const unsigned char pass[MAX_DATA]){
	unsigned char source[MAX_NAME];
	strcpy(source, clientID);
	unsigned char data[MAX_DATA];
	strcpy(data, pass);

	struct message* m = create_message((unsigned int)LOGIN, (unsigned int)strlen(pass)+1, source, data);

	char* s = convert_message_to_string(m);
	free(m);
	if(send(sockfd, s, strlen(s)+1, 0) < 0){
		return -1;
	}

	int* p_sockfd = (int*)malloc(sizeof(int));
	p_sockfd = &sockfd;
	if(pthread_create(&receiving_thread, NULL, receive,(void*) p_sockfd) != 0){
		printf("Unable to create thread.\n");
	}
	else{
		printf("Created thread.\n");
		sleep(1);
	}
	return 0;
}
int logout(int sockfd, const unsigned char clientID[MAX_NAME]){
	unsigned char source[MAX_NAME];
	strcpy(source, clientID);
	unsigned char data[1];
	data[0] = '\0';

	struct message* m = create_message(EXIT, strlen(data)+1, source, data);
	char* s = convert_message_to_string(m);
	free(m);
	send(sockfd, s, strlen(s)+1, 0);
	free(s);
	printf("Logged out successfully.\n");
	return 0;
}

int join_session(int sockfd, const unsigned char sessID[MAX_DATA]){
	unsigned char source[MAX_NAME];
	pthread_mutex_lock(&info_lock);
	strcpy(source, info->clientID);
	pthread_mutex_unlock(&info_lock);
	unsigned char data[MAX_DATA];
	strcpy(data, sessID);

	struct message* m = create_message((unsigned int)JOIN, (unsigned int)strlen(sessID)+1, source, data);
	char* s = convert_message_to_string(m);
//	printf("%s\n", s);
	free(m);

	if(send(sockfd, s, strlen(s)+1, 0) < 0){
		free(s);
		return -1;
	}	
	free(s);
	sleep(1);
	return 0;
}
int create_session(int sockfd, const unsigned char sessID[MAX_DATA]){
//	printf("sessID: %s\n", sessID);
	unsigned char source[MAX_NAME];
	pthread_mutex_lock(&info_lock);
	strcpy(source, info->clientID);
	pthread_mutex_unlock(&info_lock);
	unsigned char data[MAX_DATA];
	strcpy(data, sessID);

	struct message* m = create_message((unsigned int)NEW_SESS, (unsigned int)strlen(sessID)+1, source, data);
	char* s = convert_message_to_string(m);
	free(m);

	if(send(sockfd, s, strlen(s)+1, 0) < 0){
		free(s);
		return -1;
	}
	free(s);
	sleep(1);
	return 0;
}
int leave_session(int sockfd){
	unsigned char source[MAX_NAME];
	strcpy(source, info->clientID);
	unsigned char data[1];
	data[0] = '\0';

	struct message* m = create_message((unsigned int)LEAVE_SESS, (unsigned int)strlen(data)+1, source, data);
	char* s = convert_message_to_string(m);
//	printf("%s\n", s);
	free(m);

	if(send(sockfd, s, strlen(s)+1, 0) < 0){
		free(s);
		return -1;
	}
	free(s);
	return 0;
}
int send_message(int sockfd, const char* data){
	unsigned char source[MAX_NAME];
	pthread_mutex_lock(&info_lock);
	strcpy(source, info->clientID);
	pthread_mutex_unlock(&info_lock);
	unsigned char dataa[MAX_DATA];
	strcpy(dataa, data);

	struct message* m = create_message((unsigned int)MESSAGE, (unsigned int)strlen(dataa)+1, source, dataa);
	char* s = convert_message_to_string(m);
//	printf("%s\n", s);
	free(m);

	if(send(sockfd, s, strlen(s)+1, 0) < 0){
		free(s);
		return -1;
	}	
	free(s);
	return 0;
}
int list(int sockfd){
	unsigned char source[MAX_NAME];
	pthread_mutex_lock(&info_lock);
	strcpy(source, info->clientID);
	pthread_mutex_unlock(&info_lock);
	unsigned char data[MAX_DATA];
	strcpy(data, "");

	struct message* m = create_message((unsigned int)QUERY, (unsigned int)strlen(data)+1, source, data);
	char* s = convert_message_to_string(m);
//	printf("%s\n", s);
	free(m);

	if(send(sockfd, s, strlen(s)+1, 0) < 0){
		free(s);
		return -1;
	}	
	free(s);
	sleep(1);
	return 0;
}

// Extra features
int whisper(int sockfd, const unsigned char* data){
	unsigned char source[MAX_NAME];
	pthread_mutex_lock(&info_lock);
	strcpy(source, info->clientID);
	pthread_mutex_unlock(&info_lock);
	unsigned char dataa[MAX_DATA];
	strcpy(dataa, data);

	struct message* m = create_message((unsigned int)WHISPER, (unsigned int)strlen(data)+1, source, data);
	char* s = convert_message_to_string(m);
	free(m);

	if(send(sockfd, s, strlen(s)+1, 0) < 0){
		free(s);
		return -1;
	}	
	free(s);
	sleep(1);
	return 0;
}
int invite_user(int sockfd, const unsigned char* data){
	unsigned char source[MAX_NAME];
	pthread_mutex_lock(&info_lock);
	strcpy(source, info->clientID);
	pthread_mutex_unlock(&info_lock);
	unsigned char dataa[MAX_DATA];
	strcpy(dataa, data);

	struct message* m = create_message((unsigned int)INVITE, (unsigned int)strlen(data)+1, source, data);
	char* s = convert_message_to_string(m);
	free(m);

	if(send(sockfd, s, strlen(s)+1, 0) < 0){
		free(s);
		return -1;
	}	
	free(s);
	sleep(1);
	return 0;
}

