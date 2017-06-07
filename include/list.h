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

#ifndef	_BATTLE_LIST_H
#define	_BATTLE_LIST_H

#include <stddef.h>

#define LIST_INIT(_lst, _tp)	do {\
		(_lst)->type = _tp;\
		(_lst)->head = NULL;\
		(_lst)->cur = NULL;\
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
