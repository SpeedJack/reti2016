#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

#define LISTEN_BACKLOG	20

#define print_error(msg)	do {\
			fprintf(stderr, "%s:%d: %s(): ", \
				__FILE__, __LINE__, __func__);\
			if (errno)\
				perror(msg);\
			else\
				fprintf(stderr, "%s\n", msg);\
			errno = 0;\
		} while (0)

int set_nonblocking(int fd)
{
	int flags;

	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

uint16_t get_port(const struct sockaddr *sa)
{
	switch(sa->sa_family) {
	case AF_INET:
		return ntohs(((struct sockaddr_in *)sa)->sin_port);
	case AF_INET6:
		return ntohs(((struct sockaddr_in6 *)sa)->sin6_port);
	}
	return 0;
}

int start_listening(const struct addrinfo hints, const char *service)
{
	struct addrinfo *result, *rp;
	int s, sfd;
	uint16_t port;

	if ((s = getaddrinfo(NULL, service, &hints, &result))) {
		print_error(gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0 &&
				listen(sfd, LISTEN_BACKLOG) == 0 &&
				(port = get_port(rp->ai_addr)))
			break;

		close(sfd);
		sfd = 0;
	}

	if (!rp) {
		freeaddrinfo(result);
		return 0;
	}

	printf("Server listening on port %d\n", port);

	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char **argv)
{
	struct addrinfo hints;
	int sfd, connfd;

	if (argc != 2) {
		printf("Usage: %s <port>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV |
			AI_V4MAPPED | AI_ADDRCONFIG;

	sfd = start_listening(hints, argv[1]);
	if (!sfd) {
		print_error("Could not create socket");
		exit(EXIT_FAILURE);
	}

	connfd = accept(sfd, NULL, NULL);
	set_nonblocking(connfd);

	close(sfd);
	exit(EXIT_SUCCESS);

exit_free_sock:
	close(sfd);
	exit(EXIT_FAILURE);
	return EXIT_FAILURE;
}
