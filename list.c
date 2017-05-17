#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "console.h"
#include "list.h"

struct list_node {
	void *key;
	void *obj;
	struct list_node *next;
};

static int compare_keys(enum key_type type, void *key1, void *key2)
{
	switch (type) {
	case TP_STR:
		return strcasecmp((char *)key1, (char *)key2);
	case TP_INT:
	default:
		return *(int *)key1 - *(int *)key2;
	}
}

static void list_insert_node(struct list_head *list, struct list_node *node)
{
	struct list_node *prev;
	struct list_node *p;
	struct list_node *head;

	head = list->head;

	if (!head || compare_keys(list->type, node->key, head->key) <= 0) {
		node->next = head;
		list->head = node;
		return;
	}

	for (p = head->next, prev = head;
			p && compare_keys(list->type, node->key, p->key) > 0;
			p = p->next)
		prev = p;
	node->next = p;
	prev->next = node;
}

void list_insert(struct list_head *list, void *obj, void *key)
{
	struct list_node *node;

	if (!list)
		return;

	errno = 0;
	node = malloc(sizeof(struct list_node));
	if (!node) {
		print_error("malloc", errno);
		exit(EXIT_FAILURE);
	}

	node->key = key;
	node->obj = obj;

	list_insert_node(list, node);
}

void *list_remove(struct list_head *list, void *key)
{
	struct list_node *prev;
	struct list_node *p;
	int res;
	void *obj;

	if (!list || !list->head)
		return NULL;

	prev = NULL;
	for (p = list->head; p; p = p->next) {
		res = compare_keys(list->type, p->key, key);
		if (res >= 0)
			break;
		prev = p;
	}
	if (!p || res > 0)
		return NULL;

	if (!prev)
		list->head = p->next;
	else
		prev->next = p->next;

	if (list->cur == p)
		list->cur = p->next;

	obj = p->obj;
	free(p);
	return obj;
}

void *list_search(struct list_head *list, void *key)
{
	struct list_node *p;

	if (!list || !list->head)
		return NULL;

	p = list->head;
	while (p) {
		int res;
		res = compare_keys(list->type, p->key, key);
		if (res == 0)
			return p->obj;
		if (res > 0)
			return NULL;
		if (res < 0)
			p = p->next;
	}

	return NULL;
}

void *list_first(struct list_head *list)
{
	if (!list)
		return NULL;
	if (!list->head)
		return (list->cur = NULL);

	return (list->cur = list->head)->obj;
}

void *list_next(struct list_head *list)
{
	if (!list)
		return NULL;
	if (!list->cur || !list->cur->next)
		return (list->cur = NULL);

	return (list->cur = list->cur->next)->obj;
}
