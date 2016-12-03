#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void print_error(const char *funcname, const char *format, ...)
{
	va_list args;

	va_start(args, format);

	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");

	if (errno)
		fprintf(stderr, "%s(%d): %s\n",
				(funcname && *funcname != '\0') ? funcname : "error",
				errno,
				strerror(errno));

	va_end(args);
}

int main(int argc, char **argv)
{
	return 0;
}
