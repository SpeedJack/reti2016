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

/* 0 for ipv4 mode; 1 for ipv6 mode */
#define	USE_IPV6_ADDRESSING	0

/* constants that depends on the ip version choosen */
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
#define ADDRESS_FAMILY		AF_INET6
/* default ipv6 server address used when not specified in command line */
#define	DEFAULT_SERVER_ADDRESS	"::1"
#define	STRUCT_SOCKADDR_SIZE	sizeof(struct sockaddr_in6)
#define	ADDRESS_STRING_LENGTH	INET6_ADDRSTRLEN
#else
#define ADDRESS_FAMILY		AF_INET
/* default ipv4 server address used when not specified in command line */
#define	DEFAULT_SERVER_ADDRESS	"127.0.0.1"
#define	STRUCT_SOCKADDR_SIZE	sizeof(struct sockaddr_in)
#define	ADDRESS_STRING_LENGTH	INET_ADDRSTRLEN
#endif

/* default port used when not specified in command line */
#define	DEFAULT_SERVER_PORT	6683

/* maximum pending connections to the server */
#define	LISTEN_BACKLOG		10

/* seconds to wait before retrying to re-bind the address (used by server) */
#define	BIND_INUSE_RETRY_SECS	5


/* number of entries in the hashtable used to maintain the list of connected
 * clients (server) */
#define	HASHTABLE_SIZE		16

/* buffer sizes for various inputs (client) */
#define COMMAND_BUFFER_SIZE	100
#define UINT16_BUFFER_SIZE	10
#define	CELL_INPUT_BUFFER_SIZE	10

/* username constraints */
#define MIN_USERNAME_LENGTH	3
#define	MAX_USERNAME_LENGTH	20
#define MAX_USERNAME_SIZE	MAX_USERNAME_LENGTH+1
#define USERNAME_ALLOWED_CHARS	\
	"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_"

/* UDP port constraints */
#define	MIN_UDP_PORT_NUMBER	1024
#define	MAX_UDP_PORT_NUMBER	65535

/* set to 0 to disable colored output; 1 to enable colored output.
 * (for !show & !who) */
#define	ENABLE_COLORS		1

#if defined(ENABLE_COLORS) && ENABLE_COLORS == 1
#define COLOR_RESET		"\033[0m"
#define	COLOR_BLACK		"\033[30m"
#define	COLOR_BOLD_BLACK	"\033[1;30m"
#define	COLOR_RED		"\033[31m"
#define	COLOR_BOLD_RED		"\033[1;31m"
#define	COLOR_GREEN		"\033[32m"
#define	COLOR_BOLD_GREEN	"\033[1;32m"
#define	COLOR_YELLOW		"\033[33m"
#define	COLOR_BOLD_YELLOW	"\033[1;33m"
#define	COLOR_BLUE		"\033[34m"
#define	COLOR_BOLD_BLUE		"\033[1;34m"
#define	COLOR_MAGENTA		"\033[35m"
#define	COLOR_BOLD_MAGENTA	"\033[1;35m"
#define	COLOR_CYAN		"\033[36m"
#define	COLOR_BOLD_CYAN		"\033[1;36m"
#define	COLOR_WHITE		"\033[37m"
#define	COLOR_BOLD_WHITE	"\033[1;37m"
#else
#define COLOR_RESET		""
#define	COLOR_BLACK		""
#define	COLOR_BOLD_BLACK	""
#define	COLOR_RED		""
#define	COLOR_BOLD_RED		""
#define	COLOR_GREEN		""
#define	COLOR_BOLD_GREEN	""
#define	COLOR_YELLOW		""
#define	COLOR_BOLD_YELLOW	""
#define	COLOR_BLUE		""
#define	COLOR_BOLD_BLUE		""
#define	COLOR_MAGENTA		""
#define	COLOR_BOLD_MAGENTA	""
#define	COLOR_CYAN		""
#define	COLOR_BOLD_CYAN		""
#define	COLOR_WHITE		""
#define	COLOR_BOLD_WHITE	""
#endif
#define	COLOR_NONE		""

/* color for error messages */
#define	COLOR_ERROR		COLOR_BOLD_RED

/* colors for player status (used in !who) */
#define	COLOR_PLAYER_IDLE	COLOR_GREEN
#define	COLOR_PLAYER_IN_GAME	COLOR_RED
#define	COLOR_PLAYER_AWAITING	COLOR_BLUE

/* used for proper output alignment for the list of players (!who) */
#define	WHO_STATUS_LENGTH	37
#define	WHO_STATUS_BUFFER_SIZE	WHO_STATUS_LENGTH+1

/* precision used to check for timeouts, in seconds (client & server) */
#define	SELECT_TIMEOUT_SECONDS	3

/* timeouts in seconds */
#define	PLAY_REQUEST_TIMEOUT	60
#define	IN_GAME_TIMEOUT		60

/* game table and ships constraints */
#define	GAME_TABLE_ROWS		6
#define	GAME_TABLE_COLS		6
#define	MIN_ROW_LETTER		'A'
#define	MAX_ROW_LETTER		MIN_ROW_LETTER + GAME_TABLE_ROWS - 1
#define	MIN_COL_NUMBER		1
#define	MAX_COL_NUMBER		MIN_COL_NUMBER + GAME_TABLE_COLS - 1
#define	SHIP_COUNT		7

/* symbols for game table printing (!show) */
#define	WATER_SYMBOL		COLOR_BLUE "#" COLOR_RESET
#define	SHIP_SYMBOL		COLOR_BOLD_GREEN "@" COLOR_RESET
#define	MISS_SYMBOL		COLOR_NONE "-" COLOR_RESET
#define	SUNK_SYMBOL		COLOR_BOLD_RED "X" COLOR_RESET
