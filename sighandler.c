#include <errno.h>
#include <signal.h>
#include <string.h>
#include "console.h"
#include "sighandler.h"

static const int signums[] = {SIGHUP, SIGINT, SIGTERM, SIGUSR1, SIGUSR2, 0};
unsigned int received_signal = 0;

static void signal_handler(int signum)
{
	received_signal = signum;
}

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
