#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MIN_PORT_NUMBER	1
#define MAX_PORT_NUMBER	65535
#define LISTEN_BACKLOG	20

#define _STRINGIZE(x)	#x

#define STRINGIZE(x)	_STRINGIZE(x)

#define PRINT_LOCATION	fprintf(stderr, "%s:%s: %s(): ", __FILE__, \
		STRINGIZE(__LINE__), __func__);

#define PRINT_ERROR(msg)	if (errno) perror(msg); else \
		do { fprintf(stderr, msg); fprintf(stderr, "\n"); } while (0)

#define handle_error(msg)	do { PRINT_LOCATION PRINT_ERROR(msg); \
		exit(EXIT_FAILURE); } while (0)

int main(int argc, char **argv)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s, sfd;

	if (argc != 2) {
		printf("Usage: %s <port>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_V4MAPPED | AI_ADDRCONFIG;

	if ((s = getaddrinfo(NULL, argv[1], &hints, &result)) != 0)
		handle_error(gai_strerror(s));

	for (rp = result; rp; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0 &&
				listen(sfd, LISTEN_BACKLOG) == 0)
			break;

		close(sfd);
	}
	if (!rp)
		handle_error("Could not create socket");

	freeaddrinfo(result);

	printf("Server listening\n");

	close(sfd);
	exit(EXIT_SUCCESS);
exit_free_sock:
	close(sfd);
	exit(EXIT_FAILURE);
	return EXIT_FAILURE;
}
