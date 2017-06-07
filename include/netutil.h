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

#ifndef	_BATTLE_NETUTIL_H
#define	_BATTLE_NETUTIL_H

#include <stddef.h>
#include <netinet/in.h>

int listen_on_port(in_port_t port);
int accept_socket_connection(int sockfd, struct sockaddr_storage *sa);
int open_local_port(in_port_t port);
int bytes_available(int fd);
bool read_socket(int sockfd, bool connected, void *buf, size_t len, int flags);
bool write_socket(int sockfd, struct sockaddr_storage *dest,
		const void *buf, size_t len, int flags);
bool get_peer_address(int sockfd, char *ipstr, socklen_t size,
		in_port_t *port);
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
bool get_network_address(const char *src, struct in6_addr *dst);
void fill_sockaddr(struct sockaddr_storage *ss,
		struct in6_addr address, in_port_t port);
int connect_to_server(struct in6_addr addr, in_port_t port);
#else
bool get_network_address(const char *src, struct in_addr *dst);
void fill_sockaddr(struct sockaddr_storage *ss,
		struct in_addr address, in_port_t port);
int connect_to_server(struct in_addr addr, in_port_t port);
#endif

#endif
