#ifndef	_BATTLE_PROTO_H
#define	_BATTLE_PROTO_H

#include <netinet/in.h>

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

/* element of the flexible array used in ANS_WHO */
struct __attribute__ ((packed)) who_player {
	char username[MAX_USERNAME_SIZE];
	enum player_status status;
	char opponent[MAX_USERNAME_SIZE];
};

/* common header */
struct __attribute__ ((packed)) msg_header {
	char magic[2];
	enum msg_type type;
	char _reserved;
	uint32_t length;
};

/* generic message */
struct message {
	struct msg_header header;
	char body[];
};

/* login request (carries choosen username and port) */
struct __attribute__ ((packed)) req_login {
	struct msg_header header;
	char username[MAX_USERNAME_SIZE];
	in_port_t udp_port;
};

/* login response */
struct __attribute__ ((packed)) ans_login {
	struct msg_header header;
	enum login_response response;
};

/* list of players request (!who) */
struct __attribute__ ((packed)) req_who {
	struct msg_header header;
};

/* list of players response */
struct __attribute__ ((packed)) ans_who {
	struct msg_header header;
	struct who_player players[];
};

/* play request (!connect) */
struct __attribute__ ((packed)) req_play {
	struct msg_header header;
	char opponent[MAX_USERNAME_SIZE];
};

/* answer to the play request from a client */
struct __attribute__ ((packed)) req_play_ans {
	struct msg_header header;
	bool accept;
};

/* answer sent to both clients when a match is started
 * (carries response, address and port) */
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

/* message exchanged when clients have set up all the ships on the table */
struct __attribute__ ((packed)) msg_ready {
	struct msg_header header;
};

/* message that carries shot coordinates (!shot) */
struct __attribute__ ((packed)) msg_shot {
	struct msg_header header;
	unsigned int row;
	unsigned int col;
};

/* shot response, carries if a ship have been hit or not */
struct __attribute__ ((packed)) msg_result {
	struct msg_header header;
	bool hit;
};

/* inform the server (and the server forwards it to the other client) when a
 * a match is over */
struct __attribute__ ((packed)) msg_endgame {
	struct msg_header header;
	bool disconnected;
};

/* bad request to the server (client terminates on reception) */
struct __attribute__ ((packed)) ans_badreq {
	struct msg_header header;
};

const char *message_type_name(enum msg_type type);
void delete_message(void *msg);

struct message *read_message(int sockfd);
struct message *read_message_async(int sockfd, bool *noblock);
struct message *read_udp_message(int sockfd);
struct message *read_udp_message_async(int sockfd, bool *noblock);
struct message *read_message_type(int sockfd, enum msg_type type);

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
bool send_msg_ready(int sockfd, struct sockaddr_storage *dest);
bool send_msg_shot(int sockfd, struct sockaddr_storage *dest,
		unsigned int row, unsigned int col);
bool send_msg_result(int sockfd, struct sockaddr_storage *dest, bool hit);
#endif
