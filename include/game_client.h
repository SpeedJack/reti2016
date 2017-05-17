#ifndef	_BATTLE_GAME_CLIENT_H
#define	_BATTLE_GAME_CLIENT_H

#include <netinet/in.h>
#include "bool.h"
#include "match.h"

struct game_client {
	char username[MAX_USERNAME_SIZE];
	in_port_t port;
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
	struct in6_addr address;
#else
	struct in_addr address;
#endif
	struct match *match;
	int sock;
};

#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
struct game_client *create_client(const char *username, in_port_t in_port,
		struct in6_addr in_addr, int sock);
#else
struct game_client *create_client(const char *username, in_port_t in_port,
		struct in_addr in_addr, int sock);
#endif
void delete_client(struct game_client *client);

bool valid_username(const char *username);
bool logged_in(struct game_client *client);

#endif
