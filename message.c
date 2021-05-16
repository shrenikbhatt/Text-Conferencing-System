#include "message.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct message* create_message(const unsigned int type, const unsigned int size, const unsigned char name[MAX_NAME], const unsigned char data[MAX_DATA]){
	struct message* m = (struct message*) malloc(sizeof(struct message));
	m->type = type;
	m->size = size;

	// Assuming '\0' character does not appear in name or data before end
	strcpy(m->source, name);
	strcpy(m->data, data); 

	return m;
}

char* convert_message_to_string(struct message* message){
	char* s = (char*)malloc(sizeof(char)*(MAX_DATA+MAX_NAME+100));
	int count = 0;
	sprintf(s, "%u", message->type);
	char* col = ":";
	strcat(s, col);

	char temp[MAX_DATA];
	sprintf(temp, "%u", message->size);
	strcat(s, temp);
	strcat(s, col);

	strcpy(temp, message->source);
	strcat(s, temp);
	strcat(s, col);

	strcpy(temp, message->data);
	strcat(s, temp);
	return s;
}

struct message* convert_string_to_message(char* s){
	struct message* message = (struct message*)malloc(sizeof(struct message));
	int i = 1, count = 0;
	int parsed = 3;
	while(parsed){
		if(s[i] == ':'){
			char temp[MAX_NAME+1];
			memcpy(temp, s+count, i-count);
			temp[i-count] = '\0';
			count = i+1;
			switch(parsed){
				case 3:
					message->type = atoi(temp);
					--parsed;
					break;
				case 2:
					message->size = atoi(temp);
					--parsed;
					break;
				case 1:
					strcpy(message->source, temp);
					--parsed;
					break;
				default:
					break;
			}
		}
		++i;
	}
	memcpy(message->data, s+count, 	MAX_DATA);

	return message;

}
