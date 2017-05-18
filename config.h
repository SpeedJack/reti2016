#define	USE_IPV6_ADDRESSING	0

#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
#define ADDRESS_FAMILY		AF_INET6
#define	DEFAULT_SERVER_ADDRESS	"::1"
#define	STRUCT_SOCKADDR_SIZE	sizeof(struct sockaddr_in6)
#define	ADDRESS_STRING_LENGTH	INET6_ADDRSTRLEN
#else
#define ADDRESS_FAMILY		AF_INET
#define	DEFAULT_SERVER_ADDRESS	"127.0.0.1"
#define	STRUCT_SOCKADDR_SIZE	sizeof(struct sockaddr_in)
#define	ADDRESS_STRING_LENGTH	INET_ADDRSTRLEN
#endif

#define	DEFAULT_SERVER_PORT	6683

#define	ENABLE_COLORS		1

#if defined(ENABLE_COLORS) && ENABLE_COLORS == 1
#define COLOR_RESET		"\033[0m"
#define	COLOR_RED		"\033[31m"
#define	COLOR_BOLD_RED		"\033[1;31m"
#define	COLOR_YELLOW		"\033[33m"
#define	COLOR_BOLD_YELLOW	"\033[1;33m"
#else
#define	COLOR_RESET		""
#define	COLOR_RED		""
#define	COLOR_BOLD_RED		""
#define	COLOR_YELLOW		""
#define	COLOR_BOLD_YELLOW	""
#endif

#define	COLOR_ERROR		COLOR_BOLD_RED
#define	COLOR_PLAYER_IDLE	""
#define	COLOR_PLAYER_IN_GAME	COLOR_RED
#define	COLOR_PLAYER_AWAITING	COLOR_YELLOW

#define MIN_USERNAME_LENGTH	3
#define	MAX_USERNAME_LENGTH	20
#define MAX_USERNAME_SIZE	MAX_USERNAME_LENGTH+1
#define USERNAME_ALLOWED_CHARS	\
	"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_"

#define	WHO_STATUS_LENGTH	37
#define	WHO_STATUS_BUFFER_SIZE	WHO_STATUS_LENGTH+1

#define COMMAND_BUFFER_SIZE	100

#define	SELECT_TIMEOUT_SECONDS	3
#define	PLAY_REQUEST_TIMEOUT	60
