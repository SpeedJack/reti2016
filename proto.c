#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "bool.h"
#include "console.h"
#include "netutil.h"
#include "proto.h"

struct message *create_message(struct msg_header mh, void *data)
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
}

void delete_message(struct message *msg)
{
	if (msg)
		free(msg);
}

static bool valid_message(struct message *msg)
{
	switch(msg->header.type) { /* TODO */
	case MSG_READY:
	case MSG_SHOT:
	case MSG_RESULT:
	case MSG_ENDGAME:
	case REQ_LOGIN:
	case ANS_LOGIN:
	case REQ_WHO:
	case ANS_WHO:
	case REQ_PLAY:
	case ANS_PLAY_REQ:
	case ANS_PLAY:
	case ANS_BADREQ:
		return msg->header.magic[0] == 'B' &&
			msg->header.magic[1] == 'P';
	default:
		return false;
	}
}

bool read_message(int fd, struct message **msg)
{
	struct msg_header mh;

	*msg = NULL;

	if (!read_socket(fd, &mh, sizeof(struct msg_header)))
		return false;

	errno = 0;
	*msg = malloc(sizeof(struct msg_header) + mh.length);
	if (!*msg) {
		print_error("malloc", errno);
		return false;
	}

	(*msg)->header = mh;

	return *msg && read_socket(fd, (*msg)->body, mh.length) &&
		valid_message(*msg);
}

bool write_message(int fd, struct message *msg)
{
	msg->header.magic[0] = 'B';
	msg->header.magic[1] = 'P';
	msg->header._reserved = 0x00;
	return valid_message(msg) && write_socket(fd, msg,
			sizeof(struct msg_header) + msg->header.length);
}
