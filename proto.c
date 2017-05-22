#include <errno.h>
#include <inttypes.h>
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

#define	MSG_BODY_SIZE(_tp)	sizeof(_tp) - sizeof(struct msg_header)

#define	MSG_ZERO_FILL(_m)	memset(((struct message *)&(_m))->body, 0,\
				_m.header.length);

static const char *msg_type_name[] = {"REQ_LOGIN", "ANS_LOGIN", "REQ_WHO",
				"ANS_WHO", "REQ_PLAY", "REQ_PLAY_ANS",
				"ANS_PLAY", "MSG_READY", "MSG_SHOT",
				"MSG_RESULT", "MSG_ENDGAME", "", "", "", "",
				"ANS_BADREQ"};

inline const char *message_type_name(enum msg_type type)
{
	return msg_type_name[type & 0x0F];
}

void delete_message(void *msg)
{
	if (msg)
		free(msg);
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

	printf("%s %s (length=%" PRIu32 ") {", send ? "Sending" : "Received",
			message_type_name(msg->header.type),
			msg->header.length);

	switch (msg->header.type) {
	case REQ_LOGIN:
		printf("username=%s; udp_port=%" PRIu16,
				((struct req_login *)msg)->username,
				ntohs(((struct req_login *)msg)->udp_port));
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
		printf("response=%d; address=%s; port=%" PRIu16,
				((struct ans_play *)msg)->response,
				addrstr,
				ntohs(((struct ans_play *)msg)->udp_port));
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

static struct message *_read_message(int sockfd, bool connected)
{
	struct message *msg;
	struct msg_header mh;
	bool res;

	if (connected)
/* TODO: if server, check if 8 bytes are available before reading
 * (non-blocking) */
		res = read_socket(sockfd, &mh, sizeof(struct msg_header));
	else
		res = read_udp_socket(sockfd, &mh, sizeof(struct msg_header),
				true);
	if (!res) {
		printf_error("read_udp_message: error reading message header from socket %d",
				sockfd);
		return NULL;
	}
	if (!valid_message_header(mh)) {
		printf_error("read_udp_message: received an invalid message from socket %d",
				sockfd);
		return NULL;
	}

	errno = 0;
	msg = malloc(sizeof(struct msg_header) + mh.length);
	if (!msg) {
		print_error("malloc", errno);
		return NULL;
	}

	if (connected) {
/* TODO: if server, check if mh.length bytes are available before reading
 * (non-blocking) */
		msg->header = mh;
		res = read_socket(sockfd, msg->body, mh.length);
	} else {
		res = read_udp_socket(sockfd, msg,
				sizeof(struct msg_header) + mh.length, false);
	}
	if (res) {
#ifdef	BATTLE_SERVER
		dump_message(msg, sockfd, false);
#endif
		if (msg->header.type == ANS_BADREQ) {
			printf_error("read_udp_message: received ANS_BADREQ from socket %d",
					sockfd);
			return NULL;
		}
		return msg;
	}

	printf_error("read_udp_message: error reading message %s from socket %d",
			message_type_name(msg->header.type), sockfd);
	return NULL;
}

struct message *read_message(int sockfd)
{
	return _read_message(sockfd, true);
}

struct message *read_udp_message(int sockfd)
{
	return _read_message(sockfd, false);
}

struct message *read_message_type(int sockfd, enum msg_type type)
{
	struct message *msg;

	if ((type & 0x80) && !(type & 0xA0))
		msg = read_udp_message(sockfd);
	else
		msg = read_message(sockfd);

	if (msg && msg->header.type != type) {
		printf_error("read_message_type: received wrong message type from socket %d",
				sockfd);
		delete_message(msg);
		return NULL;
	}

	return msg;
}

static bool _write_message(int sockfd, struct message *msg,
		struct sockaddr_storage *dest)
{
	bool res;

	msg->header.magic[0] = 'B';
	msg->header.magic[1] = 'P';
	msg->header._reserved = 0x00;

#ifdef	BATTLE_SERVER
	dump_message(msg, sockfd, true);
#endif
	if (dest)
		res = write_udp_socket(sockfd, dest, msg,
				sizeof(struct msg_header) +
				msg->header.length);
	else
		res = write_socket(sockfd, msg, sizeof(struct msg_header) +
				msg->header.length);
	if (!res)
		printf_error("write_message: error writing message %s to socket %d",
				message_type_name(msg->header.type), sockfd);
	return res;
}

static inline bool write_message(int sockfd, struct message *msg)
{
	return _write_message(sockfd, msg, NULL);
}

static inline bool write_udp_message(int sockfd, struct sockaddr_storage *dest,
		struct message *msg)
{
	return _write_message(sockfd, msg, dest);
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
	msg.header.length = MSG_BODY_SIZE(struct req_who);

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
	case PLAY_OPPONENT_IN_GAME:
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

bool send_msg_ready(int sockfd, struct sockaddr_storage *dest)
{
	struct msg_ready msg;

	msg.header.type = MSG_READY;
	msg.header.length = MSG_BODY_SIZE(struct msg_ready);

	return write_udp_message(sockfd, dest, (struct message *)&msg);
}

bool send_msg_shot(int sockfd, struct sockaddr_storage *dest,
		unsigned int row, unsigned int col)
{
	struct msg_shot msg;

	msg.header.type = MSG_SHOT;
	msg.header.length = MSG_BODY_SIZE(struct msg_shot);

	msg.row = row;
	msg.col = col;

	return write_udp_message(sockfd, dest, (struct message *)&msg);
}

bool send_msg_result(int sockfd, struct sockaddr_storage *dest, bool hit)
{
	struct msg_result msg;

	msg.header.type = MSG_RESULT;
	msg.header.length = MSG_BODY_SIZE(struct msg_result);

	msg.hit = hit;

	return write_udp_message(sockfd, dest, (struct message *)&msg);
}
