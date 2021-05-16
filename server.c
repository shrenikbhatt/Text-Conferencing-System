#include "server.h"

pthread_mutex_t clients_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sessions_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t username_lock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]){
    if(argc != 2){
        printf("Usage: server <port>");
    }

    // Initialize both client and session lists to NULL
    for(int i = 0; i < MAX_CLIENTS; ++i){
        clients[i] = NULL;
    }

    for(int i = 0; i < MAX_SESSIONS; ++i){
        sessions[i] = NULL;
    }

    int sockfd;

    // TCP server side setup
    sockfd = setup(argv[1]);
    if(sockfd < 0){
        return -1;
    }
    
    // Listen on the socket
    if(listen(sockfd, MAX_CLIENTS) == -1){
        printf("Error on listen.\n");
        return -1;
    }
    printf("Listening on socket.\n");

    // Need to create a new thread for each new connection.
    while(1){
        struct user* usr = (struct user*)malloc(sizeof(struct user));
        usr->size = sizeof(usr->clientaddr);
        usr->sockfd = accept(sockfd, (struct sockaddr*) &usr->clientaddr, &usr->size);
        if(usr->sockfd == -1){
            printf("Failed to connect client.\n");
            break;
        }
        pthread_create(&(usr->thread), NULL, handle_client, (void*)usr);
    }

    return 0;
}

void* handle_client(void* temp){
    struct user* usr = (struct user*)temp;

    usr->clientID[0] = '\0';
    usr->sessID[0] = '\0';
    usr->logged = 0;

    char buffer[MAX_NAME+MAX_DATA+100];
    int bytes = 0;
    
    int cont = 1;
    while(cont){
        if((bytes = recv(usr->sockfd, buffer, MAX_NAME+MAX_DATA+100, 0)) == -1){
            printf("Error when receiving.\n");
            break;
        }
        int id = -10;

        int flag = 0;

        if(bytes == 0){
            if(strcmp(usr->sessID, "")){
                pthread_mutex_lock(&clients_lock);
                printf("User %s left session %s.\n", usr->clientID, usr->sessID);
                char s[MAX_DATA];
                strcpy(s, usr->sessID);
                strcpy(usr->sessID, "");
                for(int i = 0; i < MAX_CLIENTS; ++i){
                    if(clients[i] != NULL && !strcmp(clients[i]->sessID,s)){
                        flag = 1;
                    }
                }
                if(!flag){
                    remove_session(s);
                }
                pthread_mutex_unlock(&clients_lock);
            }
            close(usr->sockfd);
            usr->logged = 0;
            printf("Successfully logged user %s out.\n", usr->clientID);
            remove_client(usr);
            strcpy(usr->clientID, "");
            cont = 0;
            free(usr);
            break;
        }
        struct message* msg = convert_string_to_message(buffer);
        switch(msg->type){
            case LOGIN:
                if(!verify_user(msg)){
                    msg->type = LO_NAK;
                    strcpy(msg->data, "Incorrect username or password");
                    msg->size = strlen(msg->data);
                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                    cont = 0;
                }
                else if(logged_user(msg)){
                    msg->type = LO_NAK;
                    strcpy(msg->data, "User already logged in.");
                    msg->size = strlen(msg->data);
                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                    cont = 0;
                }
                else if(check_available_client()){
                    add_client(usr);
                    usr->logged = 1;
                    strcpy(usr->clientID, msg->source);
                    msg->type = LO_ACK;
                    strcpy(msg->data, "");
                    msg->size = 1;
                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                }
                else{
                    msg->type = LO_NAK;
                    strcpy(msg->data, "No Available clients");
                    msg->size = strlen(msg->data);
                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                    cont = 0;
                }
                break;
            case EXIT:
                if(strcmp(usr->sessID, "")){
                    pthread_mutex_lock(&clients_lock);
                    printf("User %s left session %s.\n", usr->clientID, usr->sessID);
                    char s[MAX_DATA];
                    strcpy(s, usr->sessID);
                    strcpy(usr->sessID, "");
                    for(int i = 0; i < MAX_CLIENTS; ++i){
                        if(clients[i] != NULL && !strcmp(clients[i]->sessID,s)){
                            flag = 1;
                        }
                    }
                    if(!flag){
                        remove_session(s);
                    }
                    pthread_mutex_unlock(&clients_lock);
                }
                close(usr->sockfd);
                usr->logged = 0;
                printf("Successfully logged user %s out.\n", usr->clientID);
                remove_client(usr);
                strcpy(usr->clientID, "");
                cont = 0;
                free(usr);
                break;
            case NEW_SESS:
                if(check_available_session() && !check_existing_session(msg->data)){
                    add_session(msg->data);
                    strcpy(usr->sessID, msg->data);
                    strcpy(msg->source, "server");
                    msg->type = NS_ACK;
                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                }
                else{
                    strcat(msg->data, " Err: Could not create session.\n");
                    strcpy(msg->source, "server");
                    msg->type = NS_NAK;
                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                }
                break;
            case JOIN:
                if(check_existing_session(msg->data) && usr->sessID[0] == '\0'){
                    strcpy(usr->sessID, msg->data);
                    strcpy(msg->source, "server");
                    msg->type = JN_ACK;
                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                }
                else if(!check_existing_session(msg->data)){
                    strcat(msg->data, " Err: Need to create session first.");
                    strcpy(msg->source, "server");
                    msg->type = JN_NAK;
                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                }
                else{
                    strcat(msg->data, " Err: User already part of another session.");
                    strcpy(msg->source, "server");
                    msg->type = JN_NAK;
                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                }
                break;
            case LEAVE_SESS:
                if(!strcmp(usr->sessID, "")){
                    printf("User is not part of any sessions.\n");
                }
                else{
                    pthread_mutex_lock(&clients_lock);
                    printf("User %s left session %s.\n", usr->clientID, usr->sessID);
                    char s[MAX_DATA];
                    strcpy(s, usr->sessID);
                    strcpy(usr->sessID, "");
                    for(int i = 0; i < MAX_CLIENTS; ++i){
                        if(clients[i] != NULL && !strcmp(clients[i]->sessID,s)){
                            flag = 1;
                        }
                    }
                    if(!flag){
                        remove_session(s);
                    }
                    pthread_mutex_unlock(&clients_lock);
                }
                break;
            case MESSAGE:
                // printf("HERE\n");
                pthread_mutex_lock(&clients_lock);
                char* s = convert_message_to_string(msg);
                for(int i = 0; i < MAX_CLIENTS; ++i){
                    if(clients[i] != NULL && strcmp(clients[i]->clientID,usr->clientID)){
                        if(!strcmp(clients[i]->sessID, usr->sessID)){
                            printf("Sent %s to %s\n", msg->data, clients[i]->clientID);
                            send(clients[i]->sockfd, s, strlen(s)+1, 0);
                        }
                    }
                }
                pthread_mutex_unlock(&clients_lock);
                free(s);
                break;
            case QUERY:
                if(1){

                    strcpy(msg->source, "server");
                    strcpy(msg->data, "\nUsers:\t\tCurr. Sess:\n");
                    msg->type = QU_ACK;
                
                    pthread_mutex_lock(&clients_lock);

                    for(int i = 0; i < MAX_CLIENTS; ++i){
                        if(clients[i] != NULL){
                            strcat(msg->data, clients[i]->clientID);
                            if(clients[i]->sessID){
                                strcat(msg->data, "\t\t");
                                strcat(msg->data, clients[i]->sessID);
                            }
                            strcat(msg->data, "\n");
                        }
                    }
                    pthread_mutex_unlock(&clients_lock);
                    
                    // strcat(msg->data, "\nSessions:\n");
                    
                    // pthread_mutex_lock(&sessions_lock);
                    // for(int i = 0; i < MAX_SESSIONS; ++i){
                    //     if(sessions[i] != NULL){
                    //         strcat(msg->data, sessions[i]);
                    //         strcat(msg->data, "\n");
                    //     }
                    // }
                    // pthread_mutex_unlock(&sessions_lock);
                    
                    msg->size = strlen(msg->data)+1;
                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                    break;
                }
            case WHISPER: ;
                char temp[MAX_DATA];
                int ii = 0;
                while(msg->data[ii] != ' ' && msg->data[ii] != '\0'){
                    ++ii;
                }
                memcpy(temp, msg->data, ii);
                temp[ii] = '\0';
                id = check_user_online(temp, msg->source);
                if(id >= 0 ){
                    char data[MAX_DATA];
                    ++ii;
                    int count = ii;
                    while(msg->data[ii] != '\0'){
                        ++ii;
                    }
                    memcpy(data, msg->data+count, ii-count);
                    data[ii-count] = '\0';
                    msg->type = WHISPER;
                    strcpy(msg->data, data);
                    msg->size = strlen(msg->data)+1;

                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    pthread_mutex_lock(&clients_lock);
                    send(clients[id]->sockfd, s, strlen(s)+1, 0);
                    pthread_mutex_unlock(&clients_lock);
                    free(s);
                }
                else{
                    msg->type = WHISP_NAK;
                    strcpy(msg->source, "server");
                    strcpy(msg->data, "Err: Requested user not online.");
                    msg->size = strlen(msg->data)+1;

                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                }
                break;
            case INVITE: ;
                id = check_user_online(msg->data, msg->source);
                printf("%d\n", id);
                if(id == -2){
                    msg->type = INVITE_NAK;
                    strcpy(msg->source, "server");
                    strcpy(msg->data, "Err: Cannot invite yourself.");
                    msg->size = strlen(msg->data)+1;

                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                }
                else if(id == -1){
                    msg->type = INVITE_NAK;
                    strcpy(msg->source, "server");
                    strcpy(msg->data, "Err: Requested user is not online.");
                    msg->size = strlen(msg->data)+1;

                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    send(usr->sockfd, s, strlen(s)+1, 0);
                    free(s);
                }
                else{
                    msg->type = INVITE;
                    strcpy(msg->data, usr->sessID);
                    msg->size = strlen(msg->data)+1;

                    char* s = convert_message_to_string(msg);
                    printf("Sending %s\n", s);
                    pthread_mutex_lock(&clients_lock);
                    send(clients[id]->sockfd, s, strlen(s)+1, 0);
                    pthread_mutex_unlock(&clients_lock);
                    free(s);
                }
                break;
            default:
                break;
        }
        strcpy(buffer, "");
        free(msg);
    }
}

int add_client(struct user* usr){
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i){
        if(clients[i] == NULL){
            clients[i] = usr;
            pthread_mutex_unlock(&clients_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return 0;
}

int remove_client(struct user* usr){
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i){
        if(clients[i] && !strcmp(clients[i]->clientID, usr->clientID)){
            clients[i] = NULL;
            pthread_mutex_unlock(&clients_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return 0;
}

int check_available_client(){
    int count = 0;
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i){
        if(clients[i] == NULL){
            ++count;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return count;
}

int check_user_online(char* user, char* curr){
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i){
        if(clients[i] && !strcmp(clients[i]->clientID, user)){
            if(!strcmp(clients[i]->clientID, curr)){
               i = -2;
            }
            pthread_mutex_unlock(&clients_lock);
            return i;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return -1;
}

int add_session(char* sessID){
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_SESSIONS; ++i){
        if(sessions[i] == NULL){
            sessions[i] = (char*)malloc(sizeof(char)*strlen(sessID));
            strcpy(sessions[i], sessID);
            pthread_mutex_unlock(&sessions_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&sessions_lock);
    return 0;
}

int remove_session(char* sessID){
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_SESSIONS; ++i){
        if(sessions[i] && !strcmp(sessions[i], sessID)){
            free(sessions[i]);
            sessions[i] = NULL;
            pthread_mutex_unlock(&sessions_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return 0;
}

int check_available_session(){
    int count = 0;
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_SESSIONS; ++i){
        if(sessions[i] == NULL){
            ++count;
        }
    }
    pthread_mutex_unlock(&sessions_lock);
    return count;
}

int check_existing_session(char* sessID){
    pthread_mutex_lock(&sessions_lock);
    for(int i = 0; i < MAX_SESSIONS; ++i){
        if(sessions[i] != NULL && !strcmp(sessions[i],sessID)){
            pthread_mutex_unlock(&sessions_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&sessions_lock);
    return 0;
}

int verify_user(struct message* msg){
    pthread_mutex_lock(&username_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i){
        if(!strcmp(usernames[i], msg->source)){
            if(!strcmp(passwords[i], msg->data)){
                pthread_mutex_unlock(&username_lock);
                return 1;
            }
            break;
        }
    }
    pthread_mutex_unlock(&username_lock);
    return 0;
}

int logged_user(struct message* msg){
    pthread_mutex_lock(&clients_lock);
    for (int i = 0; i < MAX_CLIENTS; ++i){
        if(clients[i] && !strcmp(clients[i]->clientID, msg->source) && clients[i]->logged){
            pthread_mutex_unlock(&clients_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&clients_lock);
    return 0;
}

int setup(char* port){
    int sockfd;
    struct sockaddr_in serveraddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        printf("Error creating socket.\n");
        return -1;
    }
    printf("Created socket successfully.\n");

    bzero(&serveraddr, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons(atoi(port)); 

    if(bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0){
        printf("Binding to socket failed.\n");
        serveraddr.sin_port = 0;
        if(bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) != 0){
            printf("Binding failed again.\n");
        }
    }

    socklen_t len = sizeof(serveraddr);
    if (getsockname(sockfd, (struct sockaddr *)&serveraddr, &len) == -1) {
        printf("getsockname\n");
        return -1;
    }


    printf("Binding successful.\n");
    printf("port number %d\n", ntohs(serveraddr.sin_port));

    return sockfd;
}