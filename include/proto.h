#ifndef	_BATTLE_PROTO_H
#define	_BATTLE_PROTO_H

#include <netinet/in.h>
#include "bool.h"

#define	MSG_BODY_SIZE(tp)	sizeof(tp) - sizeof(struct msg_header)

enum __attribute__ ((packed)) msg_type {
	MSG_READY	= 0x80,
	MSG_SHOT	= 0x81,
	MSG_RESULT	= 0x82,
	MSG_ENDGAME	= 0x8F,
	REQ_LOGIN	= 0x00,
	ANS_LOGIN	= 0xF0,
	REQ_WHO		= 0x01,
	ANS_WHO		= 0xF1,
	REQ_PLAY	= 0x02,
	ANS_PLAY	= 0xF2,
	ANS_BADREQ	= 0xFF
	};

enum __attribute__ ((packed)) login_response {
	LOGIN_OK,
	LOGIN_INVALID_NAME,
	LOGIN_NAME_INUSE
};

enum __attribute__ ((packed)) player_status {
	PLAYER_IDLE,
	PLAYER_AWAITING_REPLY,
	PLAYER_IN_GAME
};

enum __attribute__ ((packed)) play_response {
	PLAY_DECLINE,
	PLAY_ACCEPT,
	PLAY_TIMEDOUT
};

enum __attribute__ ((packed)) cell_status { /* move to ... */
	WATER,
	BOAT,
	MISSED,
	SUNK
};

struct __attribute__ ((packed)) who_player {
	char username[MAX_USERNAME_SIZE];
	enum player_status status;
	char opponent[MAX_USERNAME_SIZE];
};

struct __attribute__ ((packed)) msg_header {
	char magic[2];
	enum msg_type type;
	char _reserved;
	uint32_t length;
};

struct message {
	struct msg_header header;
	char body[];
};

struct __attribute__ ((packed)) req_login {
	struct msg_header header;
	char username[MAX_USERNAME_SIZE];
	in_port_t udp_port;
};

struct __attribute__ ((packed)) ans_login {
	struct msg_header header;
	enum login_response response;
};

struct __attribute__ ((packed)) req_who {
	struct msg_header header;
};


struct __attribute__ ((packed)) ans_who {
	struct msg_header header;
	struct who_player player[];
};

struct __attribute__ ((packed)) req_play {
	struct msg_header header;
	char player_username[MAX_USERNAME_SIZE];
};

struct __attribute__ ((packed)) ans_play {
	struct msg_header header;
	enum play_response response;
};

/* add msg_client_info struct that sends [username], port, addr */

struct __attribute__ ((packed)) msg_ready {
	struct msg_header header;
};

struct __attribute__ ((packed)) msg_shot {
	struct msg_header header;
	struct {
		char x;
		char y;
	} __attribute__ ((packed)) coords;
};

struct __attribute__ ((packed)) msg_result {
	bool you_win __attribute__ ((packed));
	enum cell_status result;
};

struct __attribute__ ((packed)) msg_endgame {
	struct msg_header header;
};

struct __attribute__ ((packed)) ans_badreq {
	struct msg_header header;
};

struct message *create_message(struct msg_header mh, void *data);
void delete_message(struct message *msg);

bool read_message(int fd, struct message **msg);
bool write_message(int fd, struct message *msg);

#endif
