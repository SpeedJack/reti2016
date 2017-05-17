#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "bool.h"
#include "game_client.h"
#include "client_list.h"
#include "console.h"
#include "match.h"
#include "netutil.h"
#include "proto.h"

static void send_badreq(struct game_client *client)
{
	struct ans_badreq badreq;

	badreq.header.type = ANS_BADREQ;
	badreq.header.length = MSG_BODY_SIZE(struct ans_badreq);

	printf("Sending ANS_BADREQ (length=%d) on socket %d\n",
			badreq.header.length, client->sock);

	if (!write_message(client->sock, (struct message *)&badreq))
		print_error("Error sending ANS_BADREQ message", 0);

}

static void send_play_request_timeout(struct match *m)
{
	struct ans_play ans;

	memset(&ans, 0, sizeof(struct ans_play));
	ans.header.type = ANS_PLAY;
	ans.header.length = MSG_BODY_SIZE(struct ans_play);

	ans.response = PLAY_TIMEDOUT;
	printf("Sending ANS_PLAY (length=%d) with response=%d to %s (socket: %d) and %s (socket: %d)\n",
			ans.header.length, ans.response,
			m->player1->username, m->player1->sock,
			m->player2->username, m->player2->sock);
	if (!write_message(m->player2->sock, (struct message *)&ans) ||
			!write_message(m->player1->sock, (struct message *)&ans))
		print_error("Error sending ANS_PLAY message", 0);
}

static void remove_elapsed_matches()
{
	struct game_client *p;

	for (p = first_logged_client(); p; p = next_logged_client())
		if (p->match && p->match->awaiting_reply &&
				difftime(time(NULL), p->match->request_time) >=
				PLAY_REQUEST_TIMEOUT) {
			send_play_request_timeout(p->match);
			delete_match(p->match);
		}
}

static void process_play_req_answer(struct game_client *client,
		struct ans_play_req *msg)
{
	struct ans_play ans;

	assert(client != client->match->player1);

	memset(&ans, 0, sizeof(struct ans_play));
	ans.header.type = ANS_PLAY;
	ans.header.length = MSG_BODY_SIZE(struct ans_play);

	if (msg->accept) {
		ans.response = PLAY_ACCEPT;
		ans.address = client->address;
		ans.udp_port = client->port;
		client->match->awaiting_reply = false;
	} else {
		ans.response = PLAY_DECLINE;
		delete_match(client->match);
	}

	printf("Sending ANS_PLAY (length=%d) with response=%d to %s (socket: %d)\n",
			ans.header.length, ans.response,
			client->match->player1->username,
			client->match->player1->sock);

	if (!write_message(client->match->player1->sock,
				(struct message *)&ans))
		print_error("Error sending ANS_PLAY message", 0);
}

static void process_play_request(struct game_client *client,
		struct req_play *msg)
{
	struct game_client *opponent;
	struct req_play req_forward;

	opponent = get_client_by_username(msg->opponent_username);

	if (!opponent || opponent == client) {
		struct ans_play ans;

		memset(&ans, 0, sizeof(struct ans_play));
		ans.header.type = ANS_PLAY;
		ans.header.length = MSG_BODY_SIZE(struct ans_play);

		ans.response = PLAY_INVALID_OPPONENT;

		printf("Sending ANS_PLAY (length=%d) with response=%d to %s (socket: %d)\n",
				ans.header.length, ans.response,
				client->username, client->sock);

		if (!write_message(client->sock, (struct message *)&ans))
			print_error("Error sending ANS_PLAY message", 0);

		return;
	}

	add_match(client, opponent);

	req_forward.header.type = REQ_PLAY;
	req_forward.header.length = MSG_BODY_SIZE(struct req_play);

	strncpy(req_forward.opponent_username, client->username,
			MAX_USERNAME_SIZE);

	printf("Sending REQ_PLAY (length=%d) with opponent_username=%s to %s (socket: %d)\n",
			req_forward.header.length,
			req_forward.opponent_username,
			opponent->username, opponent->sock);

	if (!write_message(opponent->sock, (struct message *)&req_forward))
		print_error("Error sending REQ_PLAY message", 0);
}

static void send_client_list(struct game_client *client)
{
	int num, i;
	struct ans_who *ans;
	struct msg_header mh;
	struct game_client *p;
	void *data;

	mh.type = ANS_WHO;
	num = logged_client_count() - 1;
	mh.length = MSG_BODY_SIZE(struct ans_who) +
		num * sizeof(struct who_player);

	data = malloc(mh.length);

	for (p = first_logged_client(), i = 0; p; p = next_logged_client()) {
		struct who_player player;

		if (p == client)
			continue;

		strncpy(player.username, p->username, MAX_USERNAME_SIZE);

		player.status = PLAYER_IDLE;
		if (p->match) {
			player.status = p->match->awaiting_reply ?
				PLAYER_AWAITING_REPLY : PLAYER_IN_GAME;
			if (p->match->player1 == p)
				strncpy(player.opponent,
						p->match->player2->username,
						MAX_USERNAME_SIZE);
			else
				strncpy(player.opponent,
						p->match->player1->username,
						MAX_USERNAME_SIZE);
		}

		memcpy(((char *)data)+i*sizeof(struct who_player), &player,
				sizeof(struct who_player));
		i++;
	}

	printf("Sending ANS_WHO (length=%d) with a list of %d players to %s (socket: %d)\n",
			mh.length, num, client->username, client->sock);

	ans = (struct ans_who *)create_message(mh, data);
	if (!write_message(client->sock, (struct message *)ans))
		print_error("Error sending ANS_WHO message", 0);
	delete_message((struct message *)ans);
	free(data);
}

static void do_login(struct game_client *client, struct req_login *msg)
{
	struct ans_login ans;

	ans.header.type = ANS_LOGIN;
	ans.header.length = MSG_BODY_SIZE(struct ans_login);

	if (!valid_username(msg->username)) {
		printf("Client on socket %d sent an invalid username: %s\n",
				client->sock, msg->username);
		ans.response = LOGIN_INVALID_NAME;
	} else if (!unique_username(msg->username)) {
		printf("Client on socket %d sent an username already in use: %s\n",
				client->sock, msg->username);
		ans.response = LOGIN_NAME_INUSE;
	} else {
		login_client(client, msg->username, msg->udp_port);
		printf("Client on socket %d is now logged in as: %s\n",
				client->sock, client->username);
		ans.response = LOGIN_OK;
	}

	printf("Sending ANS_LOGIN (length=%d) with response=%d to %s (socket: %d)\n",
			ans.header.length, ans.response,
			client->username, client->sock);

	if (!write_message(client->sock, (struct message *)&ans))
		print_error("Error sending ANS_LOGIN message", 0);
}

static bool dispatch_message(struct game_client *client)
{
	struct message *msg;

	if (!read_message(client->sock, &msg)) {
		printf_error("dispatch_message: error reading message from socket %d",
				client->sock);
		return false;
	}

	if (logged_in(client)) switch(msg->header.type) {
		case REQ_WHO:
			printf("Received REQ_WHO (length=%d) from %s on socket %d\n",
					msg->header.length, client->username,
					client->sock);
			send_client_list(client);
			break;
		case REQ_PLAY:
			printf("Received REQ_PLAY (length=%d) from %s on socket %d\n",
					msg->header.length, client->username,
					client->sock);
			process_play_request(client, (struct req_play *)msg);
			break;
		case ANS_PLAY_REQ:
			printf("Received ANS_PLAY_REQ (length=%d) from %s on socket %d\n",
					msg->header.length, client->username,
					client->sock);
			process_play_req_answer(client,
					(struct ans_play_req *)msg);
			break;
		case MSG_ENDGAME:
			printf("Received MSG_ENDGAME (length=%d) from %s on socket %d\n",
					msg->header.length, client->username,
					client->sock);
			delete_match(client->match);
			break;
		case ANS_BADREQ:
			printf_error("Received ANS_BADREQ (length=%d) from client %s on socket %d",
					msg->header.length, client->username,
					client->sock);
			delete_message(msg);
			return false;
		default:
			printf_error("Received a bad request from client %s on socket %d",
					client->username, client->sock);
			send_badreq(client);
			delete_message(msg);
			return false;
	} else switch(msg->header.type) {
		case REQ_LOGIN:
			printf("Received REQ_LOGIN (length=%d) on socket %d\n",
					msg->header.length, client->sock);
			do_login(client, (struct req_login *)msg);
			break;
		case ANS_BADREQ:
			printf_error("Received ANS_BADREQ (length=%d) on socket %d",
					msg->header.length, client->sock);
			delete_message(msg);
			return false;
		default:
			printf_error("Received a bad request on socket %d",
					client->sock);
			send_badreq(client);
			delete_message(msg);
			return false;
	}

	delete_message(msg);
	return true;
}

static int accept_connection(int sockfd)
{
	int connfd;
	struct game_client *client;
	struct sockaddr_storage addr;
	char ipstr[ADDRESS_STRING_LENGTH];
	in_port_t port;

	if (-1 == (connfd = accept_socket_connection(sockfd, &addr)))
		return -1;

#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
	client = create_client(NULL, 0,
			((struct sockaddr_in6 *)&addr)->sin6_addr, connfd);
#else
	client = create_client(NULL, 0,
			((struct sockaddr_in *)&addr)->sin_addr, connfd);
#endif
	add_client(client);

	if (get_peer_address(connfd, ipstr, ADDRESS_STRING_LENGTH, &port))
		printf("Incoming connection from %s:%d (socket: %d)\n",
				ipstr, port, connfd);

	return connfd;
}

static void dummy_handler(int signum)
{
	printf("\nReceived signal %d", signum);
}

static void close_range(int start, int stop)
{
	for (; start <= stop; start++)
		close(start);
}

static void go_server(int sfd)
{
	fd_set readfds, _readfds;
	int nfds;
	struct sigaction action;
	struct timeval timeout;

	FD_ZERO(&readfds);
	FD_SET(sfd, &readfds);
	nfds = sfd;

	client_list_init();

	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = dummy_handler;
	sigaction(SIGHUP, &action, NULL);
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	for (;;) {
		int fd, ready;

		_readfds = readfds;

		timeout.tv_sec = SELECT_TIMEOUT_SECONDS;
		timeout.tv_usec = 0;

		errno = 0;
		ready = select(nfds + 1, &_readfds, NULL, NULL, &timeout);

		if (ready == -1 && errno == EINTR) {
			client_list_destroy();
			close_range(sfd + 1, nfds);
			return;
		} else if (ready == -1) {
			print_error("select", errno);
			break;
		}

		remove_elapsed_matches();

		for (fd = 0; fd <= nfds; fd++) {
			struct game_client *client = NULL;

			if (!FD_ISSET(fd, &_readfds))
				continue;

			if (fd == sfd) {
				int connfd;

				if (-1 == (connfd = accept_connection(sfd)))
					continue;

				FD_SET(connfd, &readfds);
				nfds = (connfd > nfds) ? connfd : nfds;
				continue;
			}

			client = get_client_by_socket(fd);
			assert(client);

#define	CLOSE_CLIENT	do {\
		close(fd);\
		FD_CLR(fd, &readfds);\
		remove_client(*client);\
		delete_client(client);\
		if (fd >= nfds) {\
			nfds = get_max_fd();\
			nfds = (sfd > nfds) ? sfd : nfds;\
		}\
	} while(0)

			if (!bytes_available(fd)) {
				if (logged_in(client))
					printf("The remote host has closed the connection on socket %d (username: %s)\n",
							client->sock,
							client->username);
				else
					printf("The remote host has closed the connection on socket %d\n",
							client->sock);
				CLOSE_CLIENT;
				continue;
			}

			if (!dispatch_message(client)) {
				printf_error("dispatch_message: error. Closing connection socket %d",
						client->sock);
				CLOSE_CLIENT;
			}
		}
	}

	print_error("go_server: error. exiting...", 0);
	client_list_destroy();
	close_range(sfd + 1, nfds);
	close(sfd);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int sfd;
	uint16_t port;

	if (argc > 2) {
		printf("Usage: %s <port>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}
	if (argc < 2)
		port = DEFAULT_SERVER_PORT;
	else
		port = string_to_uint16(argv[1]);

	if (port == 0) {
		print_error("Invalid port. Enter a value between 1 and 65535\n",
				0);
		exit(EXIT_FAILURE);
	}

	sfd = listen_on_port(htons(port));
	if (sfd < 0)
		exit(EXIT_FAILURE);

	printf("Server listening on port %hu\n", port);

	go_server(sfd);

	puts("\nExiting...");

	close(sfd);
	exit(EXIT_SUCCESS);
}
