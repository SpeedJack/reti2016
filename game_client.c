#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "console.h"
#include "game_client.h"

struct match *add_match(struct game_client *p1, struct game_client *p2)
{
	struct match *m;

	m = malloc(sizeof(struct match));
	m->player1 = p1;
	m->player2 = p2;
	p1->match = m;
	p2->match = m;
	m->awaiting_reply = true;
	m->request_time = time(NULL);
	return m;
}

void delete_match(struct match *m)
{
	if (!m)
		return;

	m->player1->match = NULL;
	m->player2->match = NULL;

	free(m);
}

#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
struct game_client *create_client(const char *username, in_port_t in_port,
		struct in6_addr in_addr, int sock)
#else
struct game_client *create_client(const char *username, in_port_t in_port,
		struct in_addr in_addr, int sock)
#endif
{
	struct game_client *client;

	errno = 0;
	client = malloc(sizeof(struct game_client));
	if (!client) {
		print_error("malloc", errno);
		exit(EXIT_FAILURE);
	}

	if (username) {
		strncpy(client->username, username, MAX_USERNAME_SIZE);
		client->username[MAX_USERNAME_LENGTH] = '\0';
	} else {
		*client->username = '\0';
	}
	client->port = in_port;
	client->address = in_addr;
	client->match = NULL;
	client->sock = sock;

	return client;
}

void delete_client(struct game_client *client)
{
	if (!client)
		return;

	if (client->match)
		delete_match(client->match);

	free(client);
}

bool valid_username(const char *username)
{
	size_t i, len;

	len = strlen(username);
	if (len < MIN_USERNAME_LENGTH || len > MAX_USERNAME_LENGTH)
		return false;

	assert(username[len] == '\0');

	for (i = 0; i < len && username[i] != '\0'; i++)
		if (!strchr(USERNAME_ALLOWED_CHARS, username[i]))
			return false;

	return true;

}

bool logged_in(struct game_client *client)
{
	return (*client->username != '\0');
}
