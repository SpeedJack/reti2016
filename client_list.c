#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "client_list.h"
#include "console.h"
#include "hashtable.h"
#include "list.h"
#include "match.h"

static struct list_head *client_list = NULL;
static struct list_head client_hashtable[HASHTABLE_SIZE];
static unsigned int logged_count;

void client_list_init()
{
	if (client_list)
		return;

	errno = 0;
	client_list = malloc(sizeof(struct list_head));
	if (!client_list) {
		print_error("malloc", errno);
		exit(EXIT_FAILURE);
	}
	logged_count = 0;

	LIST_INIT(client_list, TP_STR);
	HASHTABLE_INIT(client_hashtable);
}

struct game_client *remove_client(struct game_client *client)
{
	if (client->match)
		delete_match(client->match);

	if (logged_in(client)) {
		list_remove(client_list, (void *)client->username);
		logged_count--;
	}

	return (struct game_client *)
			hashtable_remove(client_hashtable, client->sock);
}

void add_client(struct game_client *client)
{
	hashtable_insert(client_hashtable, client, &client->sock);
}

void login_client(struct game_client *client, const char *username,
		in_port_t port)
{
	strncpy(client->username, username, MAX_USERNAME_SIZE);
	client->port = port;
	list_insert(client_list, client, (void *)client->username);
	logged_count++;
}

struct game_client *get_client_by_username(const char *username)
{
	return (struct game_client *)
			list_search(client_list, (void *)username);
}

struct game_client *get_client_by_socket(int fd)
{
	return (struct game_client *)hashtable_search(client_hashtable, fd);
}

struct game_client *first_logged_client()
{
	return (struct game_client *)list_first(client_list);
}

struct game_client *next_logged_client()
{
	return (struct game_client *)list_next(client_list);
}

static struct game_client *first_client()
{
	return (struct game_client *)hashtable_first(client_hashtable);
}

static struct game_client *next_client()
{
	return (struct game_client *)hashtable_next(client_hashtable);
}

bool unique_username(const char *username)
{
	return (get_client_by_username(username) == NULL);
}

unsigned int get_max_fd()
{
	int maxfd;
	struct game_client *client;

	client = first_client();
	for (maxfd = 0; client; client = next_client())
		maxfd = (client->sock > maxfd) ? client->sock : maxfd;

	return maxfd;
}

unsigned int logged_client_count()
{
	return logged_count;
}

void client_list_destroy()
{
	struct game_client *client;

	for(client = first_client(); client; client = next_client()) {
		remove_client(client);
		delete_client(client);
	}

	free(client_list);
	client_list = NULL;
	logged_count = 0;
}
