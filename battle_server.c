/*
 * This file is part of reti2016.
 *
 * reti2016 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * reti2016 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * See file LICENSE for more details.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include "client_list.h"
#include "console.h"
#include "netutil.h"
#include "proto.h"
#include "sighandler.h"

static inline void close_range(int start, int stop)
{
	for (; start <= stop; start++)
		close(start);
}

/*
 * Removes all matches awaiting for response where the timeout is elapsed and
 * sends a message to the involved clients informing them of the timeout.
 */
static void remove_elapsed_matches()
{
	struct game_client *p;

	for (p = first_logged_client(); p; p = next_logged_client())
		if (p->match && p->match->awaiting_reply &&
				difftime(time(NULL), p->match->request_time) >=
				PLAY_REQUEST_TIMEOUT) {
			send_ans_play(p->match->player2->sock, PLAY_TIMEDOUT,
					p->match->player1->address,
					p->match->player1->port);
			send_ans_play(p->match->player1->sock, PLAY_TIMEDOUT,
					p->match->player2->address,
					p->match->player2->port);
			delete_match(p->match);
		}
}

/*
 * Deletes a match when a client disconnect from the match and informs the
 * other client that the opponent has disconnected.
 */
static void terminate_match(struct game_client *client, bool disconnected)
{
	int sockfd;

	if (!client->match)
		return;

	sockfd = (client == client->match->player1) ?
		client->match->player2->sock : client->match->player1->sock;

	if (client->match->awaiting_reply)
		send_ans_play(sockfd, (client == client->match->player1) ?
				PLAY_TIMEDOUT : PLAY_DECLINE,
				client->address, client->port);
	else
		send_msg_endgame(sockfd, disconnected);

	delete_match(client->match);
}

/* Answer to a play request */
static void process_play_req_answer(struct game_client *client,
		struct req_play_ans *msg)
{
	enum play_response res;

	if (!client->match || client == client->match->player1)
		return;

	res = msg->accept ? PLAY_ACCEPT : PLAY_DECLINE;

	send_ans_play(client->match->player1->sock, res,
			client->match->player2->address,
			client->match->player2->port);
	send_ans_play(client->match->player2->sock, res,
			client->match->player1->address,
			client->match->player1->port);

	client->match->awaiting_reply = false;
	if (!msg->accept)
		delete_match(client->match);
}

/* !connect */
static void process_play_request(struct game_client *client,
		struct req_play *msg)
{
	struct game_client *opponent;

	opponent = get_client_by_username(msg->opponent);

	if (!opponent || opponent == client) {
		send_ans_play(client->sock, PLAY_INVALID_OPPONENT,
				client->address, client->port);
		return;
	}
	if (opponent->match) {
		send_ans_play(client->sock, PLAY_OPPONENT_IN_GAME,
				client->address, client->port);
		return;
	}

	add_match(client, opponent);

	send_req_play(opponent->sock, client->username);
}

/* !who */
static void send_client_list(struct game_client *client)
{
	int count, i;
	struct game_client *p;
	struct who_player *players;
	size_t sz;

	count = logged_client_count() - 1;
	sz = count * sizeof(struct who_player);

	players = malloc(sz);
	if (!players) {
		print_error("malloc", errno);
		count = 0;
		goto send_free_and_exit;
	}
	memset(players, 0, sz);

	for (p = first_logged_client(), i = 0; p; p = next_logged_client()) {
		if (p == client)
			continue;

		strncpy(players[i].username, p->username, MAX_USERNAME_SIZE);
		players[i].username[MAX_USERNAME_LENGTH] = '\0';

		players[i].status = PLAYER_IDLE;
		if (p->match) {
			players[i].status = p->match->awaiting_reply ?
				PLAYER_AWAITING_REPLY : PLAYER_IN_GAME;
			if (p->match->player1 == p)
				strncpy(players[i].opponent,
						p->match->player2->username,
						MAX_USERNAME_SIZE);
			else
				strncpy(players[i].opponent,
						p->match->player1->username,
						MAX_USERNAME_SIZE);
			players[i].opponent[MAX_USERNAME_LENGTH] = '\0';
		}
		i++;
	}

send_free_and_exit:
	send_ans_who(client->sock, players, count);
	if (players)
		free(players);
}

static void do_login(struct game_client *client, struct req_login *msg)
{
	enum login_response res;

	if (!valid_username(msg->username)) {
		printf("Client on socket %d sent an invalid username: %s\n",
				client->sock, msg->username);
		res = LOGIN_INVALID_NAME;
	} else if (!unique_username(msg->username)) {
		printf("Client on socket %d sent an username already in use: %s\n",
				client->sock, msg->username);
		res = LOGIN_NAME_INUSE;
	} else {
		login_client(client, msg->username, msg->udp_port);
		printf("Client on socket %d is now logged in as: %s\n",
				client->sock, client->username);
		res = LOGIN_OK;
	}

	send_ans_login(client->sock, res);
}

/*
 * Dispatches a message to the correct function.
 */
static bool dispatch_message(struct game_client *client)
{
	struct message *msg;
	bool noblock;

	msg = read_message_async(client->sock, &noblock);
	if (!noblock)
		return true;
	if (!msg)
		return false;

	switch (msg->header.type) {
	case REQ_LOGIN:
		do_login(client, (struct req_login *)msg);
		break;
	case REQ_WHO:
		send_client_list(client);
		break;
	case REQ_PLAY:
		process_play_request(client, (struct req_play *)msg);
		break;
	case REQ_PLAY_ANS:
		process_play_req_answer(client,
				(struct req_play_ans *)msg);
		break;
	case MSG_ENDGAME:
		terminate_match(client,
				((struct msg_endgame *)msg)->disconnected);
		break;
	default:
		send_ans_badreq(client->sock);
	}

	delete_message(msg);
	return true;
}

/*
 * Accepts a new incoming connection.
 */
static int accept_connection(int sockfd)
{
	int connfd;
	struct sockaddr_storage addr;
	char ipstr[ADDRESS_STRING_LENGTH];
	in_port_t port;

	if (-1 == (connfd = accept_socket_connection(sockfd, &addr)))
		return -1;

#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
	add_client(((struct sockaddr_in6 *)&addr)->sin6_addr, connfd);
#else
	add_client(((struct sockaddr_in *)&addr)->sin_addr, connfd);
#endif

	if (get_peer_address(connfd, ipstr, ADDRESS_STRING_LENGTH, &port))
		printf("Incoming connection from %s:%d (socket: %d)\n",
				ipstr, port, connfd);

	return connfd;
}

/*
 * Server main cycle.
 */
static void go_server(int sfd)
{
	fd_set readfds, _readfds;
	int nfds;

	FD_ZERO(&readfds);
	FD_SET(sfd, &readfds);
	nfds = sfd;

	client_list_init();

	for (;;) {
		int fd, ready;
		struct timeval timeout;

		if (received_signal > 0) {
			client_list_destroy();
			close_range(sfd + 1, nfds);
			return;
		}

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
		terminate_match(client, true);\
		remove_client(client);\
		if (fd >= nfds) {\
			nfds = get_max_fd();\
			nfds = (sfd > nfds) ? sfd : nfds;\
		}\
	} while(0)

			if (!bytes_available(fd)) {
				if (logged_in(client))
					printf("Player %s has closed the connection on socket %d\n",
							client->username,
							client->sock);
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
		if (!string_to_uint16(argv[1], &port) || port == 0) {
			print_error("Invalid port. Enter a value between 1 and 65535\n",
				0);
			exit(EXIT_FAILURE);
		}

	sfd = listen_on_port(htons(port));
	if (sfd < 0)
		exit(EXIT_FAILURE);

	if (!sighandler_init()) {
		close(sfd);
		exit(EXIT_FAILURE);
	}

	printf("Server listening on port %hu\n", port);

	go_server(sfd);

	puts("\nExiting...");

	close(sfd);
	exit(EXIT_SUCCESS);
}
