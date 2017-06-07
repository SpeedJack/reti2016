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

#ifndef	_BATTLE_HASHTABLE_H
#define	_BATTLE_HASHTABLE_H

#include "list.h"

#define HASHTABLE_INIT(_ht)	do {\
		int __for_counter;\
		for (__for_counter = 0; __for_counter < HASHTABLE_SIZE;\
				__for_counter++)\
			LIST_INIT(&(_ht)[__for_counter], TP_INT);\
		} while(0)

int hashtable_insert(struct list_head ht[], void *obj, int *key);
void *hashtable_remove(struct list_head ht[], int key);
void *hashtable_search(struct list_head ht[], int key);

void *hashtable_first(struct list_head ht[]);
void *hashtable_next(struct list_head ht[]);


#endif
