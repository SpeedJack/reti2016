#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "console.h"
#include "game_client.h"

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

	if (username)
		strncpy(client->username, username, MAX_USERNAME_SIZE);
	else
		*client->username = '\0';
	client->port = in_port;
	client->address = in_addr;
	client->match = NULL;
	client->sock = sock;

	return client;
}

void delete_client(struct game_client *client)
{
	if (client)
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
