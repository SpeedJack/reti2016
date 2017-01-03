#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define LISTEN_BACKLOG	20
#define	ADDRSTRLEN	INET6_ADDRSTRLEN + 1

#define print_error(msg)	do {\
			fprintf(stderr, "%s:%d: %s(): ", \
				__FILE__, __LINE__, __func__);\
			if (errno)\
				perror(msg);\
			else\
				fprintf(stderr, "%s\n", msg);\
		} while (0)

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

int start_listening(const struct addrinfo hints, const char *service)
{
	struct addrinfo *result, *rp;
	int s, sfd, err;
	uint16_t port;
	char buff[ADDRSTRLEN];

	if ((s = getaddrinfo(NULL, service, &hints, &result)) != 0) {
		print_error(gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0 &&
				listen(sfd, LISTEN_BACKLOG) == 0)
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

	printf("Server listening on %s (port: %d)\n", buff, port);

	freeaddrinfo(result);
	return sfd;
}

int main(int argc, char **argv)
{
	struct addrinfo hints;
	int sfd;

	if (argc != 2) {
		printf("Usage: %s <port>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV |
			AI_V4MAPPED | AI_ADDRCONFIG;

	if (!(sfd = start_listening(hints, argv[1]))) {
		print_error("Could not create socket");
		exit(EXIT_FAILURE);
	}

	close(sfd);
	exit(EXIT_SUCCESS);

exit_free_sock:
	close(sfd);
	exit(EXIT_FAILURE);
	return EXIT_FAILURE;
}
