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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "console.h"
#include "game_client.h"
#include "netutil.h"
#include "proto.h"
#include "sighandler.h"

enum game_status {
	GAME_DISCONNECTED,
	GAME_SETUP,
	GAME_WAITING,
	GAME_MY_TURN,
	GAME_WAIT_RESULT,
	GAME_OPPONENT_TURN
};

enum game_cell {
	CELL_WATER,
	CELL_SHIP,
	CELL_MISS,
	CELL_SUNK
};

static int server_sock;
static int game_sock;
static time_t last_input; /* last input/UDP message time. Used for game
				timeout */

static struct {
	enum game_status status;
	struct {
		char username[MAX_USERNAME_LENGTH];
		enum game_cell table[GAME_TABLE_ROWS][GAME_TABLE_COLS];
		bool initiator; /* true if this player is the initiator of the
					match (has issued !connect) */
	} my;
	struct {
		char username[MAX_USERNAME_LENGTH];
		enum game_cell table[GAME_TABLE_ROWS][GAME_TABLE_COLS];
		struct sockaddr_storage sa;
	} opponent;
	struct {
		unsigned int row;
		unsigned int col;
	} fired; /* used by process_msg_result to register the result of the
			last shot */
} game;

static inline void show_help()
{
	if (game.status == GAME_DISCONNECTED)
		puts("\nAvailable commands:\n"
				"!help --> shows the list of available commands\n"
				"!who --> shows the list of connected players\n"
				"!connect username --> starts a game with the specified player\n"
				"!quit --> disconnects and exits");
	else
		puts("\nAvailable commands:\n"
				"!help --> shows the list of available commands\n"
				"!disconnect --> disconnects from the game\n"
				"!shot square --> shots the specified square\n"
				"!show --> shows the current game table");
}

static inline void print_cell_symbol(enum game_cell cell)
{
	switch (cell) {
	case CELL_WATER:
		printf("  %s", WATER_SYMBOL);
		break;
	case CELL_SHIP:
		printf("  %s", SHIP_SYMBOL);
		break;
	case CELL_MISS:
		printf("  %s", MISS_SYMBOL);
		break;
	case CELL_SUNK:
		printf("  %s", SUNK_SYMBOL);
		break;
	default:
		printf("  ?");
	}
}

/*
 * Informs the player that the game is over because the opponent has
 * disconnected or all his ships have been sunk.
 */
static void process_msg_endgame(struct msg_endgame *msg)
{
	if (game.status == GAME_DISCONNECTED || game.status == GAME_SETUP)
		return;

	if (msg->disconnected) {
		printf("%s has disconnected!", game.opponent.username);
		if (game.status != GAME_WAITING || game.my.initiator)
			putchar('\n');
	} else {
		printf("You have sunk all %s's ships! YOU WON!\n",
				game.opponent.username);
	}

	game.status = GAME_DISCONNECTED;
}

/*
 * Shows and register the result of a !shot
 */
static void process_msg_result(struct msg_result *msg)
{
	printf("%s says: %s\n", game.opponent.username, msg->hit ?
			"hit! :-)" : "miss. :-(");

	game.opponent.table[game.fired.row][game.fired.col] =
		msg->hit ? CELL_SUNK : CELL_MISS;

	game.status = GAME_OPPONENT_TURN;
}

/*
 * Checks if all ships have been sunk.
 */
static inline bool game_lost()
{
	bool lost;
	int i, j;

	for (i = 0, lost = true; i < GAME_TABLE_ROWS; i++) {
		for (j = 0; j < GAME_TABLE_COLS; j++)
			if (game.my.table[i][j] == CELL_SHIP) {
				lost = false;
				break;
			}
		if (!lost)
			break;
	}
	return lost;
}

/*
 * The opponent shots. This function answers with the result.
 */
static void process_msg_shot(struct msg_shot *msg)
{
	bool hit;

	if (msg->row > GAME_TABLE_ROWS || msg->col > GAME_TABLE_COLS) {
		print_error("Received a malformed message.", 0);
		send_msg_endgame(server_sock, true);
		return;
	}

	hit = game.my.table[msg->row][msg->col] == CELL_SHIP;

	printf("%s fires %c%d. %s\n", game.opponent.username,
			MIN_ROW_LETTER + msg->row, MIN_COL_NUMBER + msg->col,
			hit ? "Hit. :-(" : "Miss! :-)");

	game.my.table[msg->row][msg->col] = hit ? CELL_SUNK : CELL_MISS;

	if (hit && game_lost()) {
		send_msg_endgame(server_sock, false);
		puts("Oh no, all your ships have been sunk! YOU LOST!");
		game.status = GAME_DISCONNECTED;
	} else {
		send_msg_result(game_sock, &game.opponent.sa, hit);
		game.status = GAME_MY_TURN;
	}
}

/*
 * Extract the cell coords (row, col) from the null-terminated string pointed
 * by buf. buf must contains the cell in the format <row-letter><col-number>.
 * Otherwise an error is displayed and false is returned.
 */
static bool get_cell(char *buf, int *row, int *col)
{
	int ch;
	uint16_t num;
	int len;

	len = strlen(buf);
	if (len < 2) {
		print_error("Invalid format. Insert row letter followed by column number.",
				0);
		return false;
	}

	buf = trim_white_spaces(buf);
	ch = toupper((unsigned char)*buf);
	if (!isalpha((unsigned char)ch) ||
			ch < MIN_ROW_LETTER || ch > MAX_ROW_LETTER) {
		printf_error("Invalid row. Insert a row between %c and %c.",
				MIN_ROW_LETTER, MAX_ROW_LETTER);
		return false;
	}
	*row = ch - MIN_ROW_LETTER;
	if (*row < 0 || *row > GAME_TABLE_ROWS) {
		print_error("Invalid row.", 0);
		return false;
	}

	buf++;
	buf = trim_white_spaces(buf);
	if (!isdigit((unsigned char)*buf) || !string_to_uint16(buf, &num) ||
			num < MIN_COL_NUMBER || num > MAX_COL_NUMBER) {
		printf_error("Invalid column. Insert a column between %d and %d.",
				MIN_COL_NUMBER, MAX_COL_NUMBER);
		return false;
	}
	*col = num - MIN_COL_NUMBER;
	if (*col < 0 || *col > GAME_TABLE_COLS) {
		print_error("Invalid column.", 0);
		return false;
	}

	return true;
}

/* !shot */
static void shot_opponent_cell(char *buffer)
{
	int row, col;
	if (game.status != GAME_MY_TURN)
		return;

	if (!get_cell(buffer, &row, &col))
		return;

	if (game.opponent.table[row][col] != CELL_WATER) {
		print_error("You have already fired here.", 0);
		return;
	}

	game.fired.row = row;
	game.fired.col = col;
	send_msg_shot(game_sock, &game.opponent.sa, row, col);
	game.status = GAME_WAIT_RESULT;
}

/* !show */
static void show_game_tables()
{
	int i, j;
	int len;

#define MIN_NUMBER(_a,_b) ((_a) < (_b) ? (_a) : (_b))
	len = strlen(game.my.username);
	len = MIN_NUMBER(len, GAME_TABLE_COLS * 3 + 4);

	printf("\n%s", game.my.username);
	for (i = 0; i < (GAME_TABLE_COLS * 3 + 4) - len; i++)
		putchar(' ');
	printf("\t\t%s", game.opponent.username);

	fputs("\n X |", stdout);
	for (j = 0; j < GAME_TABLE_COLS; j++)
		printf("  %d", j + MIN_COL_NUMBER);
	fputs("\t\t X |", stdout);
	for (j = 0; j < GAME_TABLE_COLS; j++)
		printf("  %d", j + MIN_COL_NUMBER);
	fputs("\n---|", stdout);
	for (j = 0; j < GAME_TABLE_COLS; j++)
		fputs("---", stdout);
	fputs("\t\t---|", stdout);
	for (j = 0; j < GAME_TABLE_COLS; j++)
		fputs("---", stdout);

	for (i = 0; i < GAME_TABLE_ROWS; i++) {
		printf("\n %c |", MIN_ROW_LETTER + i);
		for (j = 0; j < GAME_TABLE_COLS; j++)
			print_cell_symbol(game.my.table[i][j]);
		printf("\t\t %c |", MIN_ROW_LETTER + i);
		for (j = 0; j < GAME_TABLE_COLS; j++)
			print_cell_symbol(game.opponent.table[i][j]);
	}

	puts("\n\n\n"	WATER_SYMBOL	" = FREE (WATER) / UNKNOWN\n"
			SHIP_SYMBOL	" = SHIP\n"
			MISS_SYMBOL	" = MISS\n"
			SUNK_SYMBOL	" = SUNK SHIP (HIT)");
}

/*
 * Asks the user to place his ships on the game table.
 */
static void place_ships()
{
	int i, len;
	int row, col;
	char buffer[CELL_INPUT_BUFFER_SIZE];

	for (row = 0; row < GAME_TABLE_ROWS; row++)
		for (col = 0; col < GAME_TABLE_COLS; col++)
			game.my.table[row][col] =
				game.opponent.table[row][col] = CELL_WATER;

	printf("\nPlace your ships one per line:\n(%d ships available; format: row letter followed by column number)\n",
			SHIP_COUNT);

	for (i = 0; i < SHIP_COUNT;) {
		printf("Ship %d: ", i + 1);
		fflush(stdout);

		len = get_line(buffer, CELL_INPUT_BUFFER_SIZE);

		if (len >= CELL_INPUT_BUFFER_SIZE) {
			print_error("Too long input.", 0);
			continue;
		}

		if (!get_cell(buffer, &row, &col))
			continue;

		if (game.my.table[row][col] == CELL_SHIP) {
			print_error("You have already placed a ship here.", 0);
			continue;
		}

		game.my.table[row][col] = CELL_SHIP;
		i++;
	}

	send_msg_ready(game_sock, &game.opponent.sa);

	printf("Waiting for %s...", game.opponent.username);
	fflush(stdout);
	game.status = GAME_WAITING;
	last_input = time(NULL);
}

/*
 * Asks the users if he wants to play with <username>
 */
static bool ask_to_play(const char *username)
{


	do {
		char c;
		fd_set readfds;
		int ready;

		FD_ZERO(&readfds);
		FD_SET(fileno(stdin), &readfds);
		FD_SET(server_sock, &readfds);

		printf("%s invited you to play a match. Accept? [Y/n] ",
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

/* Answers to a play request */
static void process_play_request(struct req_play *msg)
{
	struct ans_play *ans;
	bool accept;

	if (game.status != GAME_DISCONNECTED)
		return;

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
		strncpy(game.opponent.username, msg->opponent,
				MAX_USERNAME_SIZE);
		game.opponent.username[MAX_USERNAME_LENGTH] = '\0';
		fill_sockaddr(&game.opponent.sa, ans->address, ans->udp_port);
		game.status = GAME_SETUP;
		game.my.initiator = false;
		place_ships();
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

/* !connect */
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
		strncpy(game.opponent.username, username, MAX_USERNAME_SIZE);
		game.opponent.username[MAX_USERNAME_LENGTH] = '\0';
		fill_sockaddr(&game.opponent.sa, ans->address, ans->udp_port);
		game.status = GAME_SETUP;
		game.my.initiator = true;
		place_ships();
		delete_message(ans);
		return;
	case PLAY_INVALID_OPPONENT:
		printf("Player %s not found.\n", username);
		break;
	case PLAY_OPPONENT_IN_GAME:
		printf("%s is currently playing with another player.\n",
				username);
		break;
	case PLAY_TIMEDOUT:
		printf("%s is currently AFK. Request timed out.\n",
				username);
		break;
	default:
		print_error("Invalid response from server.", 0);
	}

	putchar('\n');
	delete_message(ans);
}

/* !who */
static void print_player_list()
{
#define _STRINGIZE(_a) #_a
#define STRINGIZE(_a) _STRINGIZE(_a)

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

/* game message dispatch */
static bool get_opponent_message()
{
	struct message *msg;

	msg = read_udp_message(game_sock);
	if (!msg)
		return false;

	switch (msg->header.type) {
	case MSG_READY:
		if (game.status != GAME_WAITING)
			return false;
		printf("%s is ready!\n", game.opponent.username);
		game.status = game.my.initiator ?
			GAME_OPPONENT_TURN : GAME_MY_TURN;
		show_help();
		putchar('\n');
		break;
	case MSG_SHOT:
		if (game.status != GAME_OPPONENT_TURN)
			return false;
		process_msg_shot((struct msg_shot *)msg);
		break;
	case MSG_RESULT:
		if (game.status != GAME_WAIT_RESULT)
			return false;
		process_msg_result((struct msg_result *)msg);
		break;
	default:
		print_error("Received an invalid message from opponent.", 0);
		delete_message(msg);
		return false;
	}

	if (game.status == GAME_MY_TURN)
		puts("It's your turn!\n");
	else if (game.status == GAME_OPPONENT_TURN)
		printf("It's %s's turn.", game.opponent.username);

	delete_message(msg);
	return true;
}

/* server message dispatch */
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
		putchar('\n');
		break;
	default:
		print_error("Received an invalid message from server.", 0);
		delete_message(msg);
		return false;
	}

	delete_message(msg);
	return true;
}

/* game mode commands dispatch */
static void process_game_command(const char *cmd, char *args)
{
	if (strcasecmp(cmd, "!help") == 0) {
		show_help();
	} else if (strcasecmp(cmd, "!shot") == 0) {
		if (args == NULL)
			print_error("!shot requires a valid game table square as argument.\n",
					0);
		else
			shot_opponent_cell(args);
		return;
	} else if (strcasecmp(cmd, "!show") == 0) {
		show_game_tables();
	} else if (strcasecmp(cmd, "!disconnect") == 0) {
		send_msg_endgame(server_sock, true);
		puts("Successfully disconnected from the game.");
		game.status = GAME_DISCONNECTED;
	} else {
		printf_error("Invalid command %s.", cmd);
	}
	putchar('\n');
}

/* normal mode commands dispatch */
static void process_std_command(const char *cmd, const char *args)
{
	if (strcasecmp(cmd, "!help") == 0) {
		show_help();
	} else if (strcasecmp(cmd, "!who") == 0) {
		print_player_list();
	} else if (strcasecmp(cmd, "!connect") == 0) {
		if (args == NULL || !valid_username(args))
			print_error("!connect requires a valid opponent name as argument.\n",
					0);
		else
			send_play_request(args);
		return;
	} else if (strcasecmp(cmd, "!quit") == 0) {
		puts("Disconnecting... Bye!");
		close(game_sock);
		close(server_sock);
		exit(EXIT_SUCCESS);
	} else {
		printf_error("Invalid command %s.", cmd);
	}
	putchar('\n');
}

/* all commands dispatch */
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
		print_error("Too long input.\n", 0);
		return;
	}

	cmd = trim_white_spaces(buffer);
	if (*cmd != '!') {
		printf_error("Invalid command %s.\n", cmd);
		return;
	}

	args = split_cmd_args(cmd);
	if (args && *args == '\0')
		args = NULL;

	if (game.status == GAME_MY_TURN)
		process_game_command(cmd, args);
	else if (game.status == GAME_DISCONNECTED)
		process_std_command(cmd, args);
	else
		flush_stdin();
}

/*
 * Client main cycle.
 */
static void wait_for_input()
{
	fd_set readfds, _readfds;
	int nfds;
	bool select_timeout;
	bool on_newline;

	FD_ZERO(&readfds);
	FD_SET(fileno(stdin), &readfds);
	FD_SET(server_sock, &readfds);
	FD_SET(game_sock, &readfds);
	nfds = (game_sock > server_sock) ? game_sock : server_sock;

	select_timeout = on_newline = false;

/* FIXME: prompt sometimes shows when it shouldn't (eg. when MSG_READY is
 * received while not in game because the opponent was AFK during the setup
 * (ship placing) phase) */
	for (;;) {
		int fd, ready;
		struct timeval timeout;

		if (received_signal)
			return;

		if (game.status == GAME_DISCONNECTED) {
			fputs(on_newline ? "> " : "\n> ", stdout);
			on_newline = false;
		} else if (!select_timeout && game.status == GAME_MY_TURN) {
			fputs(on_newline ? "# " : "\n# ", stdout);
			on_newline = false;
		}
		fflush(stdout);

		_readfds = readfds;

		timeout.tv_sec = SELECT_TIMEOUT_SECONDS;
		timeout.tv_usec = 0;

		errno = 0;
		ready = select(nfds + 1, &_readfds, NULL, NULL,
				game.status == GAME_DISCONNECTED ?
				NULL : &timeout);

		if (ready == -1 && errno == EINTR) {
			return;
		} else if (ready == -1) {
			print_error("select", errno);
			break;
		}

		select_timeout = game.status != GAME_DISCONNECTED &&
				!FD_ISSET(server_sock, &_readfds) &&
				!FD_ISSET(game_sock, &_readfds) &&
				!FD_ISSET(fileno(stdin), &_readfds);

		if (!select_timeout && !FD_ISSET(fileno(stdin), &_readfds))
			putchar('\n');

		if (game.status != GAME_DISCONNECTED && select_timeout &&
				difftime(time(NULL), last_input) >=
				IN_GAME_TIMEOUT)
		{
			send_msg_endgame(server_sock, true);
			if (game.status == GAME_WAITING)
				putchar('\n');
			puts("Disconnected for inactivity.");
			if (game.status == GAME_WAITING && game.my.initiator)
				putchar('\n');
			select_timeout = false;
			game.status = GAME_DISCONNECTED;
			continue;
		}

		for (fd = 0; fd <= nfds; fd++) {
			if (!FD_ISSET(fd, &_readfds))
				continue;

			if (fd == fileno(stdin)) {
				on_newline = true;
				process_command();
				last_input = time(NULL);
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
				if (game.status == GAME_DISCONNECTED) {
					read_udp_message(game_sock);
					continue;
				}

				if (!get_opponent_message()) {
					print_error("Received wrong message.",
							0);
					send_msg_endgame(server_sock, true);
					game.status = GAME_DISCONNECTED;
					close(game_sock);
					close(server_sock);
					exit(EXIT_FAILURE);
				}

				last_input = time(NULL);
			}
		}
	}
}

static in_port_t ask_port()
{
	uint16_t port;
	in_port_t net_port;

	do {
		printf("Insert your UDP port (number in range %d-%d): ",
				MIN_UDP_PORT_NUMBER, MAX_UDP_PORT_NUMBER);
		fflush(stdout);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
		if (!get_uint16(&port) || port < MIN_UDP_PORT_NUMBER ||
				port > MAX_UDP_PORT_NUMBER) {
#pragma GCC diagnostic pop
			printf_error("Invalid port. Port must be an integer value in the range %d-%d.",
					MIN_UDP_PORT_NUMBER,
					MAX_UDP_PORT_NUMBER);
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

static int ask_username(char *username)
{
	int len;

	do {
		fputs("Insert your username: ", stdout);
		fflush(stdout);
		len = get_line(username, MAX_USERNAME_SIZE);

		if (!valid_username(username) || len > MAX_USERNAME_LENGTH) {
			printf_error("Invalid username. Username must be at least %d characters and less than %d characters and should countain only these characters:\n%s",
					MIN_USERNAME_LENGTH,
					MAX_USERNAME_LENGTH,
					USERNAME_ALLOWED_CHARS);
			continue;
		}

		return len;
	} while (1);
}

static bool do_login()
{
	in_port_t port;
	struct ans_login *ans;

	do {
		ask_username(game.my.username);
		port = ask_port();

		if (!send_req_login(server_sock, game.my.username, port))
				return false;

		ans = (struct ans_login *)read_message_type(server_sock,
				ANS_LOGIN);
		if (!ans)
			return false;

		if (ans->response == LOGIN_OK)
			break;

		switch (ans->response) {
		case LOGIN_INVALID_NAME:
			printf_error("Invalid username. Username must be at least %d characters and less than %d characters and should countain only these characters:\n%s",
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
	printf("Successfully logged-in as %s.\n", game.my.username);

	return true;
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

	if (argc > 3) {
		printf("Usage: %s <address> <port>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}
	if (argc > 1) {
		if (inet_pton(ADDRESS_FAMILY, argv[1], &addr) != 1) {
			print_error("Invalid address", 0);
			exit(EXIT_FAILURE);
		}
	}
	if (argc > 2) {
		if (!string_to_uint16(argv[2], &port) || port == 0) {
			print_error("Invalid port. Enter a value between 1 and 65535",
					0);
			exit(EXIT_FAILURE);
		}
	} else {
		port = DEFAULT_SERVER_PORT;
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

	memset(&game, 0, sizeof(game));

	if (!do_login() || !sighandler_init()) {
		close(game_sock);
		close(server_sock);
		exit(EXIT_FAILURE);
	}

	game.status = GAME_DISCONNECTED;
	show_help();

	wait_for_input();

	puts("Exiting...");
	close(game_sock);
	close(server_sock);
	exit(EXIT_SUCCESS);
}
