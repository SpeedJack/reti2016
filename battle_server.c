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

	if (errno) {
		if (funcname && *funcname != '\0')
			fprintf(stderr, "%s(%d): ", funcname, errno);
		else
			fprintf(stderr, "error(%d): ", errno);
		fprintf(stderr, "%s\n", strerror(errno));
	}

	va_end(args);
}

int main(int argc, char **argv)
{
	return 0;
}
