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

#include <errno.h>
#include <signal.h>
#include <string.h>
#include "console.h"
#include "sighandler.h"

/* list of selected signals */
static const int signums[] = {SIGHUP, SIGINT, SIGTERM, SIGUSR1, SIGUSR2, 0};
unsigned int received_signal = 0;

static void signal_handler(int signum)
{
	received_signal = signum;
}

/*
 * Set up the signal handler for all selected signals.
 */
bool sighandler_init()
{
	struct sigaction action;
	int i;

	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = signal_handler;
	errno = 0;
	for (i = 0; signums[i] > 0; i++)
		if (sigaction(signums[i], &action, NULL) == -1) {
			print_error("sigaction", errno);
			return false;
		}
	return true;
}
