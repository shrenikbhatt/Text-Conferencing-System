#ifndef _MESSAGE_H
#define _MESSAGE_H

#define MAX_NAME 50
#define MAX_DATA 1000

enum type{LOGIN, LO_ACK, LO_NAK, EXIT, JOIN, JN_ACK, JN_NAK, LEAVE_SESS, NEW_SESS, NS_ACK, NS_NAK, MESSAGE, QUERY, QU_ACK, WHISPER, WHISP_NAK, INVITE, INVITE_NAK};

struct message {
	unsigned int type;
	unsigned int size;
	unsigned char source[MAX_NAME];
	unsigned char data[MAX_DATA];
};

struct message* create_message(const unsigned int type, const unsigned int size, const unsigned char source[MAX_NAME], const unsigned char data[MAX_DATA]);
char* convert_message_to_string(struct message* message);
struct message* convert_string_to_message(char* s);

#endif
