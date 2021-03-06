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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include "console.h"
#include "netutil.h"

/*
 * Open a new listening socket from any address on the port specified by port.
 * The socked descriptor is returned.
 */
int listen_on_port(in_port_t port)
{
	struct sockaddr_storage sa;
	int sfd;

	memset(&sa, 0, sizeof(struct sockaddr_storage));
	sa.ss_family = ADDRESS_FAMILY;
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
	((struct sockaddr_in6 *)&sa)->sin6_port = port;
#else
	((struct sockaddr_in *)&sa)->sin_port = port;
#endif

	errno = 0;
	if (-1 == (sfd = socket(sa.ss_family, SOCK_STREAM, 0))) {
		print_error("socket", errno);
		return -1;
	}

	while (bind(sfd, (struct sockaddr *)&sa,
			STRUCT_SOCKADDR_SIZE) != 0) {
		print_error("bind", errno);
		if (errno != EADDRINUSE)
			goto exit_close_sock;
		printf("(retry in %d seconds...)\n", BIND_INUSE_RETRY_SECS);
		sleep(BIND_INUSE_RETRY_SECS);
	}

	if (listen(sfd, LISTEN_BACKLOG) != 0) {
		print_error("listen", errno);
		goto exit_close_sock;
	}

	return sfd;

exit_close_sock:
	close(sfd);
	return -1;
}

/*
 * Accept a new connection on the listening socket specified by sockfd. Returns
 * the newly created connection socket and saves the client address in the
 * memory area pointed by sa.
 */
int accept_socket_connection(int sockfd, struct sockaddr_storage *sa)
{
	int connfd;
	socklen_t len;

	errno = 0;
	len = STRUCT_SOCKADDR_SIZE;
	if (-1 == (connfd = accept(sockfd, (struct sockaddr *)sa, &len))) {
		print_error("accept", errno);
		return -1;
	}

	return connfd;
}

/*
 * Creates and binds an UDP socket to the port specified by port. Returns the
 * socket descriptor.
 */
int open_local_port(in_port_t port)
{
	int lsock;
	struct sockaddr_storage sa;

	errno = 0;
	if (-1 == (lsock = socket(ADDRESS_FAMILY, SOCK_DGRAM, 0))) {
		print_error("socket", errno);
		return -1;
	}

	memset(&sa, 0, sizeof(struct sockaddr_storage));
	sa.ss_family = ADDRESS_FAMILY;
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
	((struct sockaddr_in6 *)&sa)->sin6_port = port;
	((struct sockaddr_in6 *)&sa)->sin6_addr = in6addr_any;
#else
	((struct sockaddr_in *)&sa)->sin_port = port;
	((struct sockaddr_in *)&sa)->sin_addr.s_addr = htonl(INADDR_ANY);
#endif

	if (bind(lsock, (struct sockaddr *)&sa,
				STRUCT_SOCKADDR_SIZE) != 0) {
		print_error("bind", errno);
		close(lsock);
		return -1;
	}

	return lsock;
}

/*
 * Returns how many bytes are available to read in the specified socket.
 */
int bytes_available(int fd)
{
	long bytes;

	if (ioctl(fd, FIONREAD, &bytes) < 0) {
		print_error("ioctl", errno);
		return -1;
	}

	return bytes;
}

/*
 * Reads len bytes from an TCP or UDP socket and places the result in the
 * memory area pointed by buf. Returns false on error.
 */
bool read_socket(int sockfd, bool connected, void *buf, size_t len, int flags)
{
	ssize_t read;

	if (!buf || !len)
		return true;

	errno = 0;
	if (connected)
		read = recv(sockfd, buf, len, flags | MSG_WAITALL);
	else
		read = recvfrom(sockfd, buf, len, flags | MSG_WAITALL,
				NULL, NULL);
	if (read < 0)
		print_error(connected ? "recv" : "recvfrom", errno);
	return (read == (ssize_t)len);
}

/*
 * Writes len bytes (read from the memory area pointed by buf) to a TCP or UDP
 * socket. Returns false on error.
 */
bool write_socket(int sockfd, struct sockaddr_storage *dest,
		const void *buf, size_t len, int flags)
{
	ssize_t sent;

	if (!buf || !len)
		return true;

	errno = 0;
	if (dest)
		sent = sendto(sockfd, buf, len, flags | MSG_NOSIGNAL,
				(struct sockaddr *)dest, STRUCT_SOCKADDR_SIZE);
	else
		sent = send(sockfd, buf, len, flags | MSG_NOSIGNAL);

	if (sent < 0)
		print_error(dest ? "sendto" : "send", errno);
	return (sent == (ssize_t)len);
}

/*
 * Returns the address (in the memory area pointed by ipstr) and port (in the
 * memory area pointed by port) in presentation format associated to the
 * connected socket specified by sockfd. size must contain the size of the
 * memory area pointed by ipstr.
 */
bool get_peer_address(int sockfd, char *ipstr, socklen_t size, in_port_t *port)
{
	socklen_t len;
	struct sockaddr_storage addr;

	len = sizeof(struct sockaddr_storage);
	getpeername(sockfd, (struct sockaddr *)&addr, &len);

	if (addr.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		*port = ntohs(s->sin_port);
		inet_ntop(AF_INET, &s->sin_addr, ipstr, size);
		return true;
	}
	if (addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		*port = ntohs(s->sin6_port);
		inet_ntop(AF_INET6, &s->sin6_addr, ipstr, size);
		return true;
	}

	print_error("get_peer_address: Invalid address family", 0);
	return false;
}

/*
 * Translates an address in presentation format to internal format.
 */
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
bool get_network_address(const char *src, struct in6_addr *dst)
#else
bool get_network_address(const char *src, struct in_addr *dst)
#endif
{
	if (inet_pton(ADDRESS_FAMILY, src, dst) != 1) {
		print_error("inet_pton: Can not convert address", 0);
		return false;
	}
	return true;
}

/*
 * fills the sockaddr_storage structure pointed by ss with the values
 * specified.
 */
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
void fill_sockaddr(struct sockaddr_storage *ss,
		struct in6_addr address, in_port_t port)
#else
void fill_sockaddr(struct sockaddr_storage *ss,
		struct in_addr address, in_port_t port)
#endif
{
	ss->ss_family = ADDRESS_FAMILY;
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
	memcpy(&((struct sockaddr_in6 *)ss)->sin6_addr,
			&address, sizeof(struct in6_addr));
	((struct sockaddr_in6 *)ss)->sin6_port = port;
#else
	memcpy(&((struct sockaddr_in *)ss)->sin_addr,
			&address, sizeof(struct in_addr));
	((struct sockaddr_in *)ss)->sin_port = port;
#endif
}

/*
 * Connects to a server and returns the connection socket.
 */
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
int connect_to_server(struct in6_addr addr, in_port_t port)
#else
int connect_to_server(struct in_addr addr, in_port_t port)
#endif
{
	struct sockaddr_storage sa;
	int sfd;

	memset(&sa, 0, sizeof(struct sockaddr_storage));
	sa.ss_family = ADDRESS_FAMILY;
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
	((struct sockaddr_in6 *)&sa)->sin6_addr = addr;
	((struct sockaddr_in6 *)&sa)->sin6_port = port;
#else
	((struct sockaddr_in *)&sa)->sin_addr = addr;
	((struct sockaddr_in *)&sa)->sin_port = port;
#endif

	errno = 0;
	if (-1 == (sfd = socket(sa.ss_family, SOCK_STREAM, 0))) {
		print_error("socket", errno);
		return -1;
	}

	if (connect(sfd, (struct sockaddr *)&sa, STRUCT_SOCKADDR_SIZE) == -1) {
		print_error("socket", errno);
		return -1;
	}

	return sfd;
}
