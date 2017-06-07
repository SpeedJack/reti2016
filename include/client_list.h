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

#ifndef	_BATTLE_CLIENT_LIST_H
#define	_BATTLE_CLIENT_LIST_H

#include "game_client.h"

void client_list_init();
void client_list_destroy();

#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
void add_client(struct in6_addr address, int sockfd);
#else
void add_client(struct in_addr address, int sockfd);
#endif
void login_client(struct game_client *client, const char *username,
		in_port_t port);
void remove_client(struct game_client *client);

struct game_client *get_client_by_socket(int fd);
struct game_client *get_client_by_username(const char *username);

struct game_client *first_logged_client();
struct game_client *next_logged_client();

bool unique_username(const char *username);

unsigned int get_max_fd();
unsigned int logged_client_count();

#endif
