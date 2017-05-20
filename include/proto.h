#ifndef	_BATTLE_PROTO_H
#define	_BATTLE_PROTO_H

#include <netinet/in.h>

#define	MSG_BODY_SIZE(tp)	sizeof(tp) - sizeof(struct msg_header)

enum __attribute__ ((packed)) msg_type {
	REQ_LOGIN	= 0x00,
	ANS_LOGIN	= 0xF1,
	REQ_WHO		= 0x02,
	ANS_WHO		= 0xF3,
	REQ_PLAY	= 0x04,
	REQ_PLAY_ANS	= 0x05,
	ANS_PLAY	= 0xF6,
	MSG_READY	= 0x87,
	MSG_SHOT	= 0x88,
	MSG_RESULT	= 0x89,
	MSG_ENDGAME	= 0xAA,
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
	PLAY_INVALID_OPPONENT,
	PLAY_OPPONENT_IN_GAME,
	PLAY_TIMEDOUT
};

enum cell_status { /* move to ... */
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
	struct who_player players[];
};

struct __attribute__ ((packed)) req_play {
	struct msg_header header;
	char opponent[MAX_USERNAME_SIZE];
};

struct __attribute__ ((packed)) req_play_ans {
	struct msg_header header;
	bool __attribute__ ((packed)) accept;
};

struct __attribute__ ((packed)) ans_play {
	struct msg_header header;
	enum play_response response;
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
	struct in6_addr address;
#else
	struct in_addr address;
#endif
	in_port_t udp_port;
};

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
	struct msg_header header;
	unsigned char result; /* from enum cell_status */
};

struct __attribute__ ((packed)) msg_endgame {
	struct msg_header header;
	bool disconnected __attribute__ ((packed));
};

struct __attribute__ ((packed)) ans_badreq {
	struct msg_header header;
};

struct message *read_message(int sockfd);
struct message *read_message_type(int sockfd, enum msg_type type);
void delete_message(void *msg);
const char *message_type_name(enum msg_type type);

bool send_req_login(int sockfd, const char *username, in_port_t port);
bool send_ans_login(int sockfd, enum login_response response);
bool send_req_who(int sockfd);
bool send_ans_who(int sockfd, struct who_player players[], int count);
bool send_req_play(int sockfd, const char *opponent);
bool send_req_play_ans(int sockfd, bool accept);
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
bool send_ans_play(int sockfd, enum play_response response,
		struct in6_addr addr, in_port_t port);
#else
bool send_ans_play(int sockfd, enum play_response response,
		struct in_addr addr, in_port_t port);
#endif
bool send_msg_endgame(int sockfd, bool disconnected);
bool send_ans_badreq(int sockfd);
#endif
