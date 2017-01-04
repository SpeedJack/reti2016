#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>


#define ADDRSTRLEN	INET6_ADDRSTRLEN + 1
#define USERNAME_MIN_LENGTH	3
#define USERNAME_MAX_LENGTH	32
#define USERNAME_ALLOWED_CHARS	\
	"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_"
#define USHORT_BUFFER_SIZE	10

#define print_error(msg)	do {\
			fprintf(stderr, "%s:%d: %s(): ", \
				__FILE__, __LINE__, __func__);\
			if (errno)\
				perror(msg);\
			else\
				fprintf(stderr, "%s\n", msg);\
			errno = 0;\
		} while (0)

typedef enum { false = 0, true } bool;

const char *get_addr_str(const struct sockaddr *sa, char *addr, uint16_t *port)
{
	switch(sa->sa_family) {
	case AF_INET:
		*port = ntohs(((struct sockaddr_in *)sa)->sin_port);
		return inet_ntop(AF_INET,
				&(((struct sockaddr_in *)sa)->sin_addr),
				addr, ADDRSTRLEN);
	case AF_INET6:
		*port = ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
		return inet_ntop(AF_INET6,
				&(((struct sockaddr_in6 *)sa)->sin6_addr),
				addr, ADDRSTRLEN);
	}
	return NULL;
}

int connect_to_server(const struct addrinfo hints, const char *node,
		const char *service)
{
	struct addrinfo *result, *rp;
	int s, sfd, err;
	uint16_t port;
	char buff[ADDRSTRLEN];

	if ((s = getaddrinfo(node, service, &hints, &result))) {
		print_error(gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;

		close(sfd);
		sfd = 0;
	}

	memset(&buff, '\0', ADDRSTRLEN);

	if (!rp || !get_addr_str(rp->ai_addr, buff, &port)) {
		err = errno;
		freeaddrinfo(result);
		if (sfd)
			close(sfd);
		errno = err;
		return 0;
	}

	printf("Successfully connected to server %s (port: %d)\n\n", buff, port);

	freeaddrinfo(result);
	return sfd;
}

static inline void show_help()
{
	printf("Available commands:\n"
			"!help --> shows the list of available commands\n"
			"!who --> shows the list of connected players\n"
			"!connect username --> starts a game with the player username\n"
			"!quit --> disconnects and exits\n\n");
}

int get_line(char *buffer, size_t size)
{
	int c, len;

	assert(size > 0);

	fgets(buffer, size, stdin);

	len = strlen(buffer);
	if (buffer[len-1] == '\n')
		buffer[len-1] = '\0';
	else
		while ((c = getchar()) != '\n' && c != EOF)
			len++;
	return len - 1;
}

bool get_ushort(unsigned short *num)
{
	char buffer[USHORT_BUFFER_SIZE];

	if (get_line(buffer, USHORT_BUFFER_SIZE) >= USHORT_BUFFER_SIZE)
		return false;
	if (sscanf(buffer, "%hu", num) != 1)
		return false;
	return true;
}

bool valid_username(const char *username, size_t length)
{
	size_t i;

	if (length < USERNAME_MIN_LENGTH || length > USERNAME_MAX_LENGTH)
		return false;

	assert(username[length] == '\0');

	for (i = 0; i < length && username[i] != '\0'; i++)
		if (!strchr(USERNAME_ALLOWED_CHARS, username[i]))
			return false;

	return true;
}

int open_local_port(uint16_t port)
{
	return true;
}

static void login()
{
	char username[USERNAME_MAX_LENGTH+1];
	int len;
	unsigned short portbuff;
	uint16_t port;

	do { /* TODO: move to a function */
		printf("Insert your username: ");
		len = get_line(username, USERNAME_MAX_LENGTH + 1);

		if (!valid_username(username, len)) {
			printf("Invalid username. Username must be at least %d characters and less than %d characters and can contain only these characters:\n%s\n",
					USERNAME_MIN_LENGTH,
					USERNAME_MAX_LENGTH,
					USERNAME_ALLOWED_CHARS);
			continue;
		}

		do {
			printf("Insert your UDP port for incoming connections: ");
			if (!get_ushort(&portbuff) || portbuff == 0) {
				printf("Invalid port. Port must be an integer value in the range 1-65535.\n");
				continue;
			}

			port = htons((uint16_t)portbuff);

			if (!open_local_port(port)) {
				perror("Could not open this UDP port");
				continue;
			}

			break;
		} while (1);

		break;
	} while (1);

	/* TODO */
}

int main(int argc, char **argv)
{
	struct addrinfo hints;
	int sfd;

	if (argc != 3) {
		printf("Usage: %s <host> <port>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV | AI_V4MAPPED | AI_ADDRCONFIG;

	sfd = connect_to_server(hints, argv[1], argv[2]);
	if (!sfd) {
		print_error("Could not connect to server");
		exit(EXIT_FAILURE);
	}

	show_help();

	login();

	close(sfd);
	exit(EXIT_SUCCESS);

exit_free_sock:
	close(sfd);
	exit(EXIT_FAILURE);
	return EXIT_FAILURE;
}
