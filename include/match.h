#ifndef	_BATTLE_MATCH_H
#define	_BATTLE_MATCH_H

#include <time.h>
#include "bool.h"
#include "game_client.h"

struct match {
	struct game_client *player1;
	struct game_client *player2;
	bool awaiting_reply;
	time_t request_time;
};

struct match *add_match(struct game_client *p1, struct game_client *p2);
void delete_match(struct match *match);

#endif
