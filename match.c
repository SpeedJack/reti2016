#include <stdlib.h>
#include "match.h"

struct match *add_match(struct game_client *p1, struct game_client *p2)
{
	struct match *m;

	m = malloc(sizeof(struct match));
	m->player1 = p1;
	m->player2 = p2;
	p1->match = m;
	p2->match = m;
	m->awaiting_reply = true;
	m->request_time = time(NULL);
	return m;
}

void delete_match(struct match *m)
{
	if (!m)
		return;

	m->player1->match = NULL;
	m->player2->match = NULL;

	free(m);
}
