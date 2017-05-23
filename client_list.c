#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "client_list.h"
#include "console.h"
#include "hashtable.h"
#include "list.h"

/*
 * The list contains all logged in (with username) clients, ordered
 * alphabetically; while the hashtable cointains all connected clients.
 */
static struct list_head *client_list = NULL;
static struct list_head client_hashtable[HASHTABLE_SIZE];
static unsigned int logged_count;

/*
 * Allocates the necessary space for the list and the hashtable.
 */
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

void remove_client(struct game_client *client)
{
	if (logged_in(client)) {
		list_remove(client_list, (void *)client->username);
		logged_count--;
	}

	hashtable_remove(client_hashtable, client->sock);
	delete_client(client);
}

/*
 * Creates a client (not logged in) and adds it to the hashtable.
 */
#if defined(USE_IPV6_ADDRESSING) && USE_IPV6_ADDRESSING == 1
void add_client(struct in6_addr address, int sockfd)
#else
void add_client(struct in_addr address, int sockfd)
#endif
{
	struct game_client *client;

	client = create_client(NULL, 0, address, sockfd);
	hashtable_insert(client_hashtable, client, &client->sock);
}

/*
 * Logins a client, adding an username and a port to it. It also adds the
 * client to the ordered list.
 */
void login_client(struct game_client *client, const char *username,
		in_port_t port)
{
	strncpy(client->username, username, MAX_USERNAME_SIZE);
	client->username[MAX_USERNAME_LENGTH] = '\0';
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

/*
 * Checks if the username passed as argument is not already used by another
 * client.
 */
bool unique_username(const char *username)
{
	return (get_client_by_username(username) == NULL);
}

/*
 * Returns the maximum socket file descriptor of the clients in the list.
 */
unsigned int get_max_fd()
{
	int maxfd;
	struct game_client *client;

	client = first_client();
	for (maxfd = 0; client; client = next_client())
		maxfd = (client->sock > maxfd) ? client->sock : maxfd;

	return maxfd;
}

/*
 * Returns the total number of logged in (with username) clients.
 */
unsigned int logged_client_count()
{
	return logged_count;
}

/*
 * Deletes all remaining allocated data in the list.
 */
void client_list_destroy()
{
	struct game_client *client;

	for (client = first_client(); client; client = next_client())
		remove_client(client);

	free(client_list);
	client_list = NULL;
	logged_count = 0;
}
