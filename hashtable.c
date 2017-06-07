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

#include "hashtable.h"

static int cur_index;

static int compute_hash(int key)
{
	return (key % HASHTABLE_SIZE);
}

int hashtable_insert(struct list_head ht[], void *obj, int *key)
{
	int h;
	h = compute_hash(*key);
	list_insert(&ht[h], obj, (void *)key);
	return h;
}

void *hashtable_remove(struct list_head ht[], int key)
{
	return list_remove(&ht[compute_hash(key)], (void *)&key);
}

void *hashtable_search(struct list_head ht[], int key)
{
	return list_search(&ht[compute_hash(key)], (void *)&key);
}

static void *get_next(struct list_head ht[], void *n)
{
	while (!n) {
		cur_index++;
		if (cur_index == HASHTABLE_SIZE)
			return NULL;
		n = list_first(&ht[cur_index]);
	}
	return n;
}

void *hashtable_first(struct list_head ht[])
{
	void *n;

	cur_index = 0;
	n = list_first(&ht[cur_index]);
	return get_next(ht, n);
}

void *hashtable_next(struct list_head ht[])
{
	void *n;

	n = list_next(&ht[cur_index]);
	return get_next(ht, n);
}
