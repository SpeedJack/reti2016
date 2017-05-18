#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "console.h"
#include "netutil.h"
#include "proto.h"
#ifdef	BATTLE_SERVER
#include <arpa/inet.h>
#include "client_list.h"
#include "game_client.h"
#endif

const char *msg_type_name[] = {"REQ_LOGIN", "ANS_LOGIN", "REQ_WHO", "ANS_WHO",
				"REQ_PLAY", "REQ_PLAY_ANS", "ANS_PLAY",
				"MSG_READY", "MSG_SHOT", "MSG_RESULT",
				"MSG_ENDGAME", "", "", "", "", "ANS_BADREQ"};

#define	MSG_ZERO_FILL(m)	memset(((struct message *)&m)->body, 0,\
				m.header.length);

/*struct message *create_message(struct msg_header mh, void *data)
{
	struct message *msg;

	errno = 0;
	msg = malloc(sizeof(struct msg_header) + mh.length);
	if (!msg) {
		print_error("malloc", errno);
		return NULL;
	}

	msg->header = mh;
	memcpy(msg->body, data, mh.length);

	return msg;
}*/

void delete_message(struct message *msg)
{
	if (msg)
		free(msg);
}

inline const char *message_type_name(enum msg_type type)
{
	return msg_type_name[type & 0x0F];
}

static bool valid_message_header(struct msg_header mh)
{
	if (mh.magic[0] != 'B' || mh.magic[1] != 'P')
		return false;

	switch (mh.type) {
	case REQ_LOGIN:
		return mh.length == MSG_BODY_SIZE(struct req_login);
	case ANS_LOGIN:
		return mh.length == MSG_BODY_SIZE(struct ans_login);
	case REQ_WHO:
		return mh.length == MSG_BODY_SIZE(struct req_who);
	case ANS_WHO:
		return (mh.length % sizeof(struct who_player)) == 0;
	case REQ_PLAY:
		return mh.length == MSG_BODY_SIZE(struct req_play);
	case REQ_PLAY_ANS:
		return mh.length == MSG_BODY_SIZE(struct req_play_ans);
	case ANS_PLAY:
		return mh.length == MSG_BODY_SIZE(struct ans_play);
	case MSG_READY:
		return mh.length == MSG_BODY_SIZE(struct msg_ready);
	case MSG_SHOT:
		return mh.length == MSG_BODY_SIZE(struct msg_shot);
	case MSG_RESULT:
		return mh.length == MSG_BODY_SIZE(struct msg_result);
	case MSG_ENDGAME:
		return mh.length == MSG_BODY_SIZE(struct msg_endgame);
	case ANS_BADREQ:
		return mh.length == MSG_BODY_SIZE(struct ans_badreq);
	}

	return false;
}

#ifdef	BATTLE_SERVER
static void dump_message(struct message *msg, int sockfd, bool send)
{
	struct game_client *client;
	char addrstr[ADDRESS_STRING_LENGTH] = "<error>";

	client = get_client_by_socket(sockfd);

	printf("%s %s (length=%u) {", send ? "Sending" : "Received",
			message_type_name(msg->header.type),
			msg->header.length);

	switch (msg->header.type) {
	case REQ_LOGIN:
		printf("username=%s; udp_port=%hu",
				((struct req_login *)msg)->username,
				((struct req_login *)msg)->udp_port);
		break;
	case ANS_LOGIN:
		printf("response=%d", ((struct ans_login *)msg)->response);
		break;
	case ANS_WHO:
		printf("... (n. of players: %lu) ...",
				msg->header.length /
				sizeof(struct who_player));
		break;
	case REQ_PLAY:
		printf("opponent=%s", ((struct req_play *)msg)->opponent);
		break;
	case REQ_PLAY_ANS:
		printf("accept=%s", ((struct req_play_ans *)msg)->accept ?
				"true" : "false");
		break;
	case ANS_PLAY:
		inet_ntop(ADDRESS_FAMILY, &((struct ans_play *)msg)->address,
				addrstr, ADDRESS_STRING_LENGTH);
		printf("response=%d; address=%s; port=%hu",
				((struct ans_play *)msg)->response,
				addrstr, ((struct ans_play *)msg)->udp_port);
		break;
	case MSG_ENDGAME:
		printf("disconnected=%s",
				((struct msg_endgame *)msg)->disconnected ?
				"true" : "false");
		break;
	case REQ_WHO:
	case ANS_BADREQ:
		fputs("... (empty) ...", stdout);
		break;
	default:
		fputs("???", stdout);
	}

	printf("} %s ", send ? "to" : "from");
	if (logged_in(client))
		printf("%s on ", client->username);
	printf("socket %d\n", sockfd);
}
#endif

struct message *read_message(int sockfd) /* TODO: nonblocking */
{
	struct message *msg;
	struct msg_header mh;

	if (!read_socket(sockfd, &mh, sizeof(struct msg_header))) {
		printf_error("read_message: error reading message header from socket %d",
				sockfd);
		return NULL;
	}
	if (!valid_message_header(mh)) {
		printf_error("read_message: received an invalid message from socket %d",
				sockfd);
		return NULL;
	}

	errno = 0;
	msg = malloc(sizeof(struct msg_header) + mh.length);
	if (!msg) {
		print_error("malloc", errno);
		return NULL;
	}

	msg->header = mh;

	if (read_socket(sockfd, msg->body, mh.length)) {
#ifdef	BATTLE_SERVER
		dump_message(msg, sockfd, false);
#endif
		return msg;
	}

	printf_error("read_message: error reading message %s from socket %d",
			message_type_name(msg->header.type), sockfd);
	return NULL;
}

static bool write_message(int sockfd, struct message *msg)
{
	msg->header.magic[0] = 'B';
	msg->header.magic[1] = 'P';
	msg->header._reserved = 0x00;

#ifdef	BATTLE_SERVER
	dump_message(msg, sockfd, true);
#endif
	if (write_socket(sockfd, msg, sizeof(struct msg_header) + msg->header.length))
		return true;

	printf_error("write_message: error writing message %s to socket %d",
			message_type_name(msg->header.type), sockfd);
	return false;
}

bool send_req_login(int sockfd, const char *username, in_port_t port)
{
	struct req_login msg;

	msg.header.type = REQ_LOGIN;
	msg.header.length = MSG_BODY_SIZE(struct req_login);

	MSG_ZERO_FILL(msg);

	strncpy(msg.username, username, MAX_USERNAME_SIZE);
	msg.username[MAX_USERNAME_LENGTH] = '\0';
	msg.udp_port = port;

	return write_message(sockfd, (struct message *)&msg);
}

bool send_ans_login(int sockfd, enum login_response response)
{
	struct ans_login msg;

	msg.header.type = ANS_LOGIN;
	msg.header.length = MSG_BODY_SIZE(struct ans_login);

	switch (response) {
	case LOGIN_OK:
	case LOGIN_INVALID_NAME:
	case LOGIN_NAME_INUSE:
		msg.response = response;
		break;
	default:
		print_error("send_ans_login: invalid response", 0);
		return false;
	}

	return write_message(sockfd, (struct message *)&msg);
}

bool send_req_who(int sockfd)
{
	struct req_who msg;

	msg.header.type = REQ_WHO;
	msg.header.length = MSG_BODY_SIZE(struct req_login);

	return write_message(sockfd, (struct message *)&msg);
}

bool send_ans_who(int sockfd, struct who_player players[], int count)
{
	struct ans_who *msg;
	size_t array_size;
	bool res;

	array_size = count * sizeof(struct who_player);

	errno = 0;
	msg = malloc(sizeof(struct msg_header) + array_size);
	if (!msg) {
		print_error("malloc", errno);
		return false;
	}

	msg->header.type = ANS_WHO;
	msg->header.length = array_size;

	if (players && array_size > 0)
		memcpy(msg->players, players, array_size);

	res = write_message(sockfd, (struct message *)msg);
	free(msg);
	return res;
}

bool send_req_play(int sockfd, const char *opponent)
{
	struct req_play msg;

	msg.header.type = REQ_PLAY;
	msg.header.length = MSG_BODY_SIZE(struct req_play);

	MSG_ZERO_FILL(msg);

	strncpy(msg.opponent, opponent, MAX_USERNAME_SIZE);
	msg.opponent[MAX_USERNAME_LENGTH] = '\0';

	return write_message(sockfd, (struct message *)&msg);
}

bool send_req_play_ans(int sockfd, bool accept)
{
	struct req_play_ans msg;

	msg.header.type = REQ_PLAY_ANS;
	msg.header.length = MSG_BODY_SIZE(struct req_play_ans);

	msg.accept = accept;

	return write_message(sockfd, (struct message *)&msg);
}

#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
bool send_ans_play(int sockfd, enum play_response response,
		struct in6_addr addr, in_port_t port)
#else
bool send_ans_play(int sockfd, enum play_response response,
		struct in_addr addr, in_port_t port)
#endif
{
	struct ans_play msg;

	msg.header.type = ANS_PLAY;
	msg.header.length = MSG_BODY_SIZE(struct ans_play);

	MSG_ZERO_FILL(msg);

	switch (response) {
	case PLAY_ACCEPT:
		msg.address = addr;
		msg.udp_port = port;
	case PLAY_DECLINE:
	case PLAY_INVALID_OPPONENT:
	case PLAY_TIMEDOUT:
		msg.response = response;
		break;
	default:
		print_error("send_ans_play: invalid response", 0);
		return false;
	}

	return write_message(sockfd, (struct message *)&msg);
}

bool send_msg_endgame(int sockfd, bool disconnected)
{
	struct msg_endgame msg;

	msg.header.type = MSG_ENDGAME;
	msg.header.length = MSG_BODY_SIZE(struct msg_endgame);

	msg.disconnected = disconnected;

	return write_message(sockfd, (struct message *)&msg);
}

bool send_ans_badreq(int sockfd)
{
	struct ans_badreq msg;

	msg.header.type = ANS_BADREQ;
	msg.header.length = MSG_BODY_SIZE(struct ans_badreq);

	return write_message(sockfd, (struct message *)&msg);
}
