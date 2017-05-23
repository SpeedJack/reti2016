#ifndef	_BATTLE_GAME_CLIENT_H
#define	_BATTLE_GAME_CLIENT_H

#include <netinet/in.h>

struct match;

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

struct match {
	struct game_client *player1;
	struct game_client *player2;
	bool awaiting_reply;
	time_t request_time;
};

struct match *add_match(struct game_client *p1, struct game_client *p2);
void delete_match(struct match *match);

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
