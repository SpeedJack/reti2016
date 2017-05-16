#ifndef	_BATTLE_NETUTIL_H
#define	_BATTLE_NETUTIL_H

#include <stddef.h>
#include <netinet/in.h>

int set_nonblocking_socket(int fd);
int listen_on_port(in_port_t port);
int accept_socket_connection(int sockfd, struct sockaddr_storage *sa);
int open_local_port(in_port_t port);
int bytes_available(int fd);
bool read_socket(int sockfd, void *buf, size_t len);
bool write_socket(int sockfd, const void *buf, size_t len);
bool get_peer_address(int sockfd, char *ipstr, socklen_t size,
		in_port_t *port);
#if ADDRESS_FAMILY == AF_INET6
bool get_network_address(const char *src, struct in6_addr *dst);
int connect_to_server(struct in6_addr addr, in_port_t port);
#else
bool get_network_address(const char *src, struct in_addr *dst);
int connect_to_server(struct in_addr addr, in_port_t port);
#endif

#endif
