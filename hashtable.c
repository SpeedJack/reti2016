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
