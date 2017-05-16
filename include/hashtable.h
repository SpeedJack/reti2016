#ifndef	_BATTLE_HASHTABLE_H
#define	_BATTLE_HASHTABLE_H

#include "list.h"

#define	HASHTABLE_SIZE	16

#define HASHTABLE_INIT(ht)	do {\
		int __for_counter;\
		for (__for_counter = 0; __for_counter < HASHTABLE_SIZE;\
				__for_counter++)\
			LIST_INIT(&ht[__for_counter], TP_INT);\
		} while(0)

int hashtable_insert(struct list_head ht[], void *obj, int *key);
void *hashtable_remove(struct list_head ht[], int key);
void *hashtable_search(struct list_head ht[], int key);

void *hashtable_first(struct list_head ht[]);
void *hashtable_next(struct list_head ht[]);


#endif
