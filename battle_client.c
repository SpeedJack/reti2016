#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "bool.h"
#include "console.h"
#include "game_client.h"
#include "netutil.h"
#include "proto.h"

static bool in_game;
static int server_sock;
static int game_sock;

static inline void show_help()
{
	puts("\nAvailable commands:\n"
			"!help --> shows the list of available commands\n"
			"!who --> shows the list of connected players\n"
			"!connect username --> starts a game with the player username\n"
			"!quit --> disconnects and exits");
}

static bool ask_to_play(const char *username)
{
	char c;

	do {
		printf("\nPlayer %s invited you to play a match. Accept? [Y/n] ",
				username);
		c = get_character();

		switch (c) {
		case 'y':
		case 'Y':
		case '\n':
			return true;
		case 'n':
		case 'N':
			return false;
		default:
			puts("Invalid answer.");
		}
	} while (1);
}

static int ask_username(char *username)
{
	int len;

	do {
		printf("Insert your username: ");
		fflush(stdout);
		len = get_line(username, MAX_USERNAME_SIZE);

		if (!valid_username(username) || len > MAX_USERNAME_LENGTH) {
			printf("Invalid username. Username must be at leas %d characters and less than %d characters and should countain only these characters:\n%s\n",
					MIN_USERNAME_LENGTH,
					MAX_USERNAME_LENGTH,
					USERNAME_ALLOWED_CHARS);
			continue;
		}

		return len;
	} while (1);
}

static uint16_t ask_port()
{
	uint16_t port;

	do {
		printf("Insert your UDP port for incoming connections: ");
		fflush(stdout);
		if (!get_uint16(&port) || port == 0) {
			puts("Invalid port. Port must be an integer value in the range 1-65535.");
			continue;
		}

		if (-1 == (game_sock = open_local_port(htons(port)))) {
			puts("Can not open this port.");
			continue;
		}

		return port;
	} while (1);
}

static bool do_login()
{
	char username[MAX_USERNAME_SIZE];
	uint16_t port;
	struct req_login req;
	struct message *ans;

	do {
		ask_username(username);
		port = ask_port();

		req.header.type = REQ_LOGIN;
		req.header.length = MSG_BODY_SIZE(struct req_login);

		strncpy(req.username, username, MAX_USERNAME_SIZE);
		req.udp_port = port;

		if (!write_message(server_sock, (struct message *)&req)) {
			print_error("Error sending REQ_LOGIN message.", 0);
			return false;
		}

		if (!read_message(server_sock, &ans) ||
				ans->header.type != ANS_LOGIN) {
			print_error("Error reading message ANS_LOGIN from server.",
					0);
			delete_message(ans);
			return false;
		}

		if (((struct ans_login *)ans)->response == LOGIN_OK)
			break;

		switch(((struct ans_login *)ans)->response) {
		case LOGIN_INVALID_NAME:
			printf("Invalid username. Username must be at leas %d characters and less than %d characters and should countain only these characters:\n%s\n",
					MIN_USERNAME_LENGTH,
					MAX_USERNAME_LENGTH,
					USERNAME_ALLOWED_CHARS);
			break;
		case LOGIN_NAME_INUSE:
			printf("This username is already in use by another player. Please choose another username.\n");
			break;
		default:
			print_error("Invalid response from server.", 0);
			delete_message(ans);
			return false;
		}

		delete_message(ans);
		close(game_sock);
	} while (1);

	delete_message(ans);
	printf("Successfully logged-in as %s.\n", username);

	return true;
}

static void print_player_list()
{
#define _STRINGIZE(a) #a
#define STRINGIZE(a) _STRINGIZE(a)

	struct req_who req;
	struct ans_who *ans;
	int i, count;

	req.header.type = REQ_WHO;
	req.header.length = MSG_BODY_SIZE(struct req_who);

	if (!write_message(server_sock, (struct message *)&req)) {
		print_error("Error sending REQ_WHO message.", 0);
		return;
	}

	if (!read_message(server_sock, (struct message **)&ans) ||
			ans->header.type != ANS_WHO) {
		print_error("Error reading message ANS_WHO from server.", 0);
		delete_message((struct message *)ans);
		return;
	}

	assert((ans->header.length % sizeof(struct who_player)) == 0);
	count = ans->header.length / sizeof(struct who_player);

	if (count == 0) {
		puts("There are no connected players.");
		delete_message((struct message *)ans);
		return;
	}

	printf("\n%-" STRINGIZE(MAX_USERNAME_LENGTH) "s\t"
			"%" STRINGIZE(WHO_STATUS_LENGTH) "s\n\n",
			"USERNAME", "STATUS");
	for (i = 0; i < count; i++) {
		char status[WHO_STATUS_BUFFER_SIZE];

		switch (ans->player[i].status) {
		case PLAYER_AWAITING_REPLY:
			fputs(COLOR_PLAYER_AWAITING, stdout);
			snprintf(status, WHO_STATUS_BUFFER_SIZE,
					"AWAITING REPLY (%s)",
					ans->player[i].opponent);
			break;
		case PLAYER_IN_GAME:
			fputs(COLOR_PLAYER_IN_GAME, stdout);
			snprintf(status, WHO_STATUS_BUFFER_SIZE,
					"IN GAME (%s)",
					ans->player[i].opponent);
			break;
		case PLAYER_IDLE:
		default:
			fputs(COLOR_PLAYER_IDLE, stdout);
			strcpy(status, "IDLE");
		}

		printf("%-" STRINGIZE(MAX_USERNAME_LENGTH) "s\t%"
				STRINGIZE(WHO_STATUS_LENGTH) "s\n",
				ans->player[i].username, status);

		fputs(COLOR_RESET, stdout);
	}

	delete_message((struct message *)ans);
}

static void process_play_request(struct req_play *msg)
{
	struct ans_play_req ans;

	ans.header.type = ANS_PLAY_REQ;
	ans.header.length = MSG_BODY_SIZE(struct ans_play_req);

	in_game = ans.accept = ask_to_play(msg->opponent_username);

	if (!write_message(server_sock, (struct message *)&ans)) {
		print_error("Error sending ANS_PLAY_REQ message.", 0);
		in_game = false;
	}
}

static void send_play_request(const char *username)
{
	struct req_play req;
	struct ans_play *ans;

	req.header.type = REQ_PLAY;
	req.header.length = MSG_BODY_SIZE(struct req_play);

	strncpy(req.opponent_username, username, MAX_USERNAME_SIZE);

	if (!write_message(server_sock, (struct message *)&req)) {
		print_error("Error sending REQ_PLAY message.", 0);
		return;
	}

	if (!read_message(server_sock, (struct message **)&ans) ||
			ans->header.type != ANS_PLAY) {
		print_error("Error reading message ANS_PLAY from server.", 0);
		delete_message((struct message *)ans);
		return;
	}

	switch (ans->response) {
	case PLAY_DECLINE:
		printf("%s declined the invite to play.\n", username);
		break;
	case PLAY_ACCEPT:
		printf("%s accepted the invite to play!\n", username);
		in_game = true;
		break;
	case PLAY_INVALID_OPPONENT:
		printf("Player %s not found.\n", username);
		break;
	case PLAY_TIMEDOUT:
		printf("Player %s is currently AFK. Request timed out.\n",
				username);
		break;
	default:
		print_error("Invalid response from server.", 0);
	}

	delete_message((struct message *)ans);
}

static void process_std_command(const char *cmd, const char *args)
{
	if (strcasecmp(cmd, "!help") == 0) {
		show_help();
	} else if (strcasecmp(cmd, "!who") == 0) {
		print_player_list();
	} else if (strcasecmp(cmd, "!connect") == 0) {
		if (args == NULL || !valid_username(args))
			print_error("!connect requires a valid opponent name as argument.",
					0);
		else
			send_play_request(args);
	} else if (strcasecmp(cmd, "!quit") == 0) {
		puts("Disconnecting... Bye!");
		close(game_sock);
		close(server_sock);
		exit(EXIT_SUCCESS);
	} else {
		printf_error("Invalid command %s.", cmd);
	}
}

static void process_game_command(const char *cmd, const char *args)
{
	/* dummy */
	if (strcasecmp(cmd, "!help") == 0) {
		show_help();
	} else if (strcasecmp(cmd, "!shot") == 0) {
		if (args == NULL || false /* invalid square */)
			print_error("!shot requires a valid game table square as argument.",
					0);
		else
			{;} /* shot */
	} else if (strcasecmp(cmd, "!show") == 0) {
		; /* show */
	} else if (strcasecmp(cmd, "!disconnect") == 0) {
		close(game_sock);
		; /* send MSG_ENDGAME */
		puts("Disconnected from game: YOU GAVE UP!");
		in_game = false;
	} else {
		printf_error("Invalid command %s.", cmd);
	}
}

static void process_command()
{
	char buffer[COMMAND_BUFFER_SIZE];
	char *cmd;
	char *args;
	int len;

	len = get_line(buffer, COMMAND_BUFFER_SIZE);
	if (len > COMMAND_BUFFER_SIZE - 1) {
		print_error("Too long input.", 0);
		return;
	}

	cmd = trim_white_spaces(buffer);
	if (*cmd != '!') {
		printf_error("Invalid command %s.", cmd);
		return;
	}

	args = split_cmd_args(cmd);
	if (args && *args == '\0')
		args = NULL;

	if (in_game)
		process_game_command(cmd, args);
	else
		process_std_command(cmd, args);
}

static bool get_server_message()
{
	struct message *msg;

	if (!read_message(server_sock, &msg)) {
		print_error("get_server_message: error reading message from server.", 0);
		return false;
	}

	if (msg->header.type == REQ_PLAY) {
		process_play_request((struct req_play *)msg);
		return true;
	}

	print_error("Received an invalid message from server.", 0);
	delete_message(msg);
	return false;
}

static bool get_opponent_message()
{
	return true;
}

static void wait_for_input()
{
	fd_set readfds, _readfds;
	int nfds;

	FD_ZERO(&readfds);
	FD_SET(fileno(stdin), &readfds);
	FD_SET(server_sock, &readfds);
	FD_SET(game_sock, &readfds);
	nfds = (game_sock > server_sock) ? game_sock : server_sock;

	in_game = false;

	for (;;) {
		int fd, ready;

		putchar('\n');
		if (in_game)
			putchar('#');
		else
			putchar('>');
		putchar(' ');
		fflush(stdout);

		_readfds = readfds;

		errno = 0;
		ready = select(nfds + 1, &_readfds, NULL, NULL, NULL);

		if (ready == -1 && errno == EINTR) {
			continue;
		} else if (ready == -1) {
			print_error("\nselect", errno);
			break;
		}

		for (fd = 0; fd <= nfds; fd++) {
			if (!FD_ISSET(fd, &_readfds))
				continue;

			if (fd == fileno(stdin)) {
				process_command();
				continue;
			}

			if (fd == server_sock) {
				if (!bytes_available(fd)) {
					printf("\nThe server has closed the connection.\n");
					return;
				}

				if (!get_server_message()) {
					print_error("\nget_server_message: error. Closing connection.",
							0);
					close(game_sock);
					close(server_sock);
					exit(EXIT_FAILURE);
				}
				continue;
			}

			if (fd == game_sock) {
				if (!in_game) {
					print_error("\nReceived data on UDP socket when not in game.",
							0);
					continue;
				}

				if (!get_opponent_message()) {
					print_error("\nget_opponent_message: error. Closing connection.",
							0);
					close(game_sock);
					close(server_sock);
					exit(EXIT_FAILURE);
				}
			}
		}
	}
}

int main(int argc, char **argv)
{
	uint16_t port;
	char ipstr[ADDRESS_STRING_LENGTH];
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
	struct in6_addr addr;
#else
	struct in_addr addr;
#endif

	if (argc > 3 || argc == 2) {
		printf("Usage: %s <address> <port>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}
	if (argc < 2) {
		if (inet_pton(ADDRESS_FAMILY, DEFAULT_SERVER_ADDRESS,
					&addr) != 1) {
			print_error("Invalid address DEFAULT_SERVER_ADDRESS",
					0);
			exit(EXIT_FAILURE);
		}
		port = DEFAULT_SERVER_PORT;
	} else {
		if (inet_pton(ADDRESS_FAMILY, argv[1], &addr) != 1) {
			print_error("Invalid address", 0);
			exit(EXIT_FAILURE);
		}
		port = string_to_uint16(argv[2]);
		if (port == 0) {
			print_error("Invalid port. Enter a value between 1 and 65535\n",
					0);
			exit(EXIT_FAILURE);
		}
	}

	server_sock = connect_to_server(addr, htons(port));
	if (server_sock == -1) {
		print_error("Could not connect to server", 0);
		exit(EXIT_FAILURE);
	}

	if (!get_peer_address(server_sock, ipstr, ADDRESS_STRING_LENGTH,
				&port)) {
		close(server_sock);
		exit(EXIT_FAILURE);
	}

	printf("Successfully connected to server %s:%d (socket: %d)\n",
			ipstr, port, server_sock);

	if (!do_login()) {
		close(game_sock);
		close(server_sock);
		exit(EXIT_FAILURE);
	}

	show_help();

	wait_for_input();

	puts("\nExiting...");
	close(game_sock);
	close(server_sock);
	exit(EXIT_SUCCESS);
}
