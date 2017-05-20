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
#include "console.h"
#include "game_client.h"
#include "netutil.h"
#include "proto.h"
#include "sighandler.h"

static bool in_game;
static int server_sock;
static int game_sock;
static char game_opponent[MAX_USERNAME_SIZE];

static inline void show_help()
{
	if (in_game)
		puts("\nAvailable commands:\n"
				"!help --> shows the list of available commands\n"
				"!disconnect --> disconnects from the game\n"
				"!shot square --> shots the specified square\n"
				"!show --> shows the current game table");
	else
		puts("\nAvailable commands:\n"
				"!help --> shows the list of available commands\n"
				"!who --> shows the list of connected players\n"
				"!connect username --> starts a game with the specified player\n"
				"!quit --> disconnects and exits");
}

static bool ask_to_play(const char *username)
{


	do {
		char c;
		fd_set readfds;
		int ready;

		FD_ZERO(&readfds);
		FD_SET(fileno(stdin), &readfds);
		FD_SET(server_sock, &readfds);

		printf("Player %s invited you to play a match. Accept? [Y/n] ",
				username);
		fflush(stdout);

		errno = 0;
		ready = select(server_sock + 1, &readfds, NULL, NULL, NULL);

		if (ready == -1 && errno == EINTR) {
			puts("\nExiting...");
			break;
		} else if (ready == -1) {
			print_error("\nselect", errno);
			break;
		}

		if (FD_ISSET(server_sock, &readfds)) {
			putchar('\n');
			if (!bytes_available(server_sock)) {
				printf("The server has closed the connection.\n");
				break;
			}
			return false;
		}

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
			print_error("Invalid answer.", 0);
		}
	} while (1);

	close(game_sock);
	close(server_sock);
	exit(EXIT_SUCCESS);
}

static int ask_username(char *username)
{
	int len;

	do {
		fputs("Insert your username: ", stdout);
		fflush(stdout);
		len = get_line(username, MAX_USERNAME_SIZE);

		if (!valid_username(username) || len > MAX_USERNAME_LENGTH) {
			printf_error("Invalid username. Username must be at leas %d characters and less than %d characters and should countain only these characters:\n%s",
					MIN_USERNAME_LENGTH,
					MAX_USERNAME_LENGTH,
					USERNAME_ALLOWED_CHARS);
			continue;
		}

		return len;
	} while (1);
}

static in_port_t ask_port()
{
	uint16_t port;
	in_port_t net_port;

	do {
		printf("Insert your UDP port for incoming connections (number in range %d-%d): ",
				MIN_UDP_PORT_NUMBER, MAX_UDP_PORT_NUMBER);
		fflush(stdout);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
		if (!get_uint16(&port) || port < MIN_UDP_PORT_NUMBER ||
				port > MAX_UDP_PORT_NUMBER) {
#pragma GCC diagnostic pop
			printf_error("Invalid port. Port must be an integer value in the range %d-%d.",
					MIN_UDP_PORT_NUMBER, MAX_UDP_PORT_NUMBER);
			continue;
		}

		net_port = htons(port);

		if (-1 == (game_sock = open_local_port(net_port))) {
			print_error("Can not open this port.", 0);
			continue;
		}

		return net_port;
	} while (1);
}

static bool do_login()
{
	char username[MAX_USERNAME_SIZE];
	in_port_t port;
	struct ans_login *ans;

	do {
		ask_username(username);
		port = ask_port();

		if (!send_req_login(server_sock, username, port))
				return false;

		ans = (struct ans_login *)read_message_type(server_sock,
				ANS_LOGIN);
		if (!ans)
			return false;

		if (ans->response == LOGIN_OK)
			break;

		switch (ans->response) {
		case LOGIN_INVALID_NAME:
			printf_error("Invalid username. Username must be at leas %d characters and less than %d characters and should countain only these characters:\n%s",
					MIN_USERNAME_LENGTH,
					MAX_USERNAME_LENGTH,
					USERNAME_ALLOWED_CHARS);
			break;
		case LOGIN_NAME_INUSE:
			print_error("This username is already in use by another player. Please choose another username.",
					0);
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

static void process_msg_endgame(struct msg_endgame *msg)
{
	if (!in_game)
		return;

	if (msg->disconnected)
		printf("Player %s has disconnected!\n", game_opponent);
	else
		printf("You have sunk all %s's ships! YOU WON!\n", game_opponent);

	in_game = false;
}

static void print_player_list()
{
#define _STRINGIZE(a) #a
#define STRINGIZE(a) _STRINGIZE(a)

	struct ans_who *ans;
	int i, count;

	if (!send_req_who(server_sock))
		return;

	ans = (struct ans_who *)read_message(server_sock);
	if (!ans)
		return;

	count = ans->header.length / sizeof(struct who_player);

	if (count == 0) {
		puts("There are no connected players.");
		delete_message(ans);
		return;
	}

	printf("\n%-" STRINGIZE(MAX_USERNAME_LENGTH) "s\t"
			"%" STRINGIZE(WHO_STATUS_LENGTH) "s\n\n",
			"USERNAME", "STATUS");
	for (i = 0; i < count; i++) {
		char status[WHO_STATUS_BUFFER_SIZE];

		switch (ans->players[i].status) {
		case PLAYER_AWAITING_REPLY:
			fputs(COLOR_PLAYER_AWAITING, stdout);
			snprintf(status, WHO_STATUS_BUFFER_SIZE,
					"AWAITING REPLY (%s)",
					ans->players[i].opponent);
			break;
		case PLAYER_IN_GAME:
			fputs(COLOR_PLAYER_IN_GAME, stdout);
			snprintf(status, WHO_STATUS_BUFFER_SIZE,
					"IN GAME (%s)",
					ans->players[i].opponent);
			break;
		case PLAYER_IDLE:
		default:
			fputs(COLOR_PLAYER_IDLE, stdout);
			strcpy(status, "IDLE");
		}

		printf("%-" STRINGIZE(MAX_USERNAME_LENGTH) "s\t%"
				STRINGIZE(WHO_STATUS_LENGTH) "s\n",
				ans->players[i].username, status);

		fputs(COLOR_RESET, stdout);
	}

	delete_message(ans);
}

static void process_play_request(struct req_play *msg)
{
	struct ans_play *ans;
	bool accept;

	accept = ask_to_play(msg->opponent);

	if (!bytes_available(server_sock) &&
			!send_req_play_ans(server_sock, accept))
		return;

	ans = (struct ans_play *)read_message_type(server_sock, ANS_PLAY);
	if (!ans)
		return;

	switch (ans->response) {
	case PLAY_DECLINE:
		if (accept)
			printf("The opponent has closed the connection.\n");
		else
			printf("You refused the game!\n");
		break;
	case PLAY_ACCEPT:
		printf("You are now playing with %s!\n", msg->opponent);
		strncpy(game_opponent, msg->opponent, MAX_USERNAME_SIZE);
		in_game = true;
		break;
	case PLAY_TIMEDOUT:
		puts("Request timed out.");
		break;
	case PLAY_INVALID_OPPONENT:
	case PLAY_OPPONENT_IN_GAME:
	default:
		print_error("Invalid response from server.", 0);
	}

	delete_message(ans);
}

static void send_play_request(const char *username)
{
	struct ans_play *ans;

	if (!send_req_play(server_sock, username))
		return;

	printf("Waiting for response from %s...\n", username);

	ans = (struct ans_play *)read_message_type(server_sock, ANS_PLAY);
	if (!ans)
		return;

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
	case PLAY_OPPONENT_IN_GAME:
		printf("Player %s is currently playing with another player.\n",
				username);
		break;
	case PLAY_TIMEDOUT:
		printf("Player %s is currently AFK. Request timed out.\n",
				username);
		break;
	default:
		print_error("Invalid response from server.", 0);
	}

	delete_message(ans);
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
		send_msg_endgame(server_sock, true);
		puts("Successfully disconnected from the game.");
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
	if (!len)
		return;
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

	msg = read_message(server_sock);
	if (!msg)
		return false;

	switch (msg->header.type) {
	case REQ_PLAY:
		process_play_request((struct req_play *)msg);
		break;
	case MSG_ENDGAME:
		process_msg_endgame((struct msg_endgame *)msg);
		break;
	default:
		print_error("Received an invalid message from server.", 0);
		delete_message(msg);
		return false;
	}

	delete_message(msg);
	return true;
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
		bool on_newline;

		if (received_signal)
			return;

		printf("\n%c ", in_game ? '#' : '>');
		fflush(stdout);

		_readfds = readfds;

		errno = 0;
		ready = select(nfds + 1, &_readfds, NULL, NULL, NULL);

		if (ready == -1 && errno == EINTR) {
			return;
		} else if (ready == -1) {
			print_error("\nselect", errno);
			break;
		}

		on_newline = false;
		for (fd = 0; fd <= nfds; fd++) {
			if (!FD_ISSET(fd, &_readfds))
				continue;

			if (!on_newline &&
					!FD_ISSET(fileno(stdin), &_readfds)) {
				putchar('\n');
				on_newline = true;
			}

			if (fd == fileno(stdin)) {
				process_command();
				continue;
			}

			if (fd == server_sock) {
				if (!bytes_available(fd)) {
					printf("The server has closed the connection.\n");
					return;
				}

				if (!get_server_message()) {
					close(game_sock);
					close(server_sock);
					exit(EXIT_FAILURE);
				}
				continue;
			}

			if (fd == game_sock) {
				if (!in_game) {
					print_error("Received data on UDP socket when not in game.",
							0);
					continue;
				}

				if (!get_opponent_message()) {
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
		if (!string_to_uint16(argv[2], &port) || port == 0) {
			print_error("Invalid port. Enter a value between 1 and 65535",
					0);
			exit(EXIT_FAILURE);
		}
	}

	if (!sighandler_init())
		exit(EXIT_FAILURE);

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
	putchar('\n');

	wait_for_input();

	puts("\nExiting...");
	close(game_sock);
	close(server_sock);
	exit(EXIT_SUCCESS);
}
