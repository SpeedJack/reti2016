#ifndef	_BATTLE_LIST_H
#define	_BATTLE_LIST_H

#include <stddef.h>

#define LIST_INIT(list, tp)	do {\
		(list)->type = tp;\
		(list)->head = NULL;\
		(list)->cur = NULL;\
	} while(0)

enum key_type { TP_INT, TP_STR };

struct list_node;

struct list_head {
	enum key_type type;
	struct list_node *head;
	struct list_node *cur;
};

void list_insert(struct list_head *list, void *obj, void *key);
void *list_remove(struct list_head *list, void *key);
void *list_search(struct list_head *list, void *key);

void *list_first(struct list_head *list);
void *list_next(struct list_head *list);

#endif
