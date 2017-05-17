#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "console.h"

#define UINT16_BUFFER_SIZE	10

uint16_t string_to_uint16(const char *str)
{
	char *garbage = NULL;
	long value = 0;

	errno = 0;

	value = strtol(str, &garbage, 0);

	if (errno || (garbage && *garbage != '\0') ||
				value < 1 || value > (1 << 16) - 1)
		return 0;
	return (uint16_t)value;
}

int get_line(char *buffer, size_t size)
{
	int c, len;

	assert(size > 0);

	fgets(buffer, size, stdin);

	len = strlen(buffer);
	if (buffer[len-1] == '\n')
		buffer[len-1] = '\0';
	else
		while ((c = getchar()) != '\n' && c != EOF)
			len++;
	return len - 1;
}

bool get_uint16(uint16_t *result)
{
	char buffer[UINT16_BUFFER_SIZE];

	if (get_line(buffer, UINT16_BUFFER_SIZE) >= UINT16_BUFFER_SIZE)
		return false;
	*result = string_to_uint16(buffer);
	return *result != 0;
}

char get_character()
{
	char ch;

	do {
		ch = getchar();
	} while (isspace(ch));

	return ch;
}

char *trim_white_spaces(char *str)
{
	char *end;

	while (isspace((unsigned char)*str)) str++;

	if (*str == '\0')
		return str;

	end  = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) end--;

	*(end + 1) = '\0';
	return str;
}

char *split_cmd_args(char *cmd)
{
	while (*cmd && !isspace((unsigned char)*cmd)) cmd++;
	if (*cmd == '\0')
		return NULL;
	*cmd = '\0';

	return trim_white_spaces(cmd + 1);
}

void print_error(const char *errstr, int errnum)
{
	fputs(COLOR_ERROR, stderr);
	fputs(errstr, stderr);
	if (errnum) {
		fputs(": ", stderr);
		fputs(strerror(errnum), stderr);
	}
	fputs(COLOR_RESET "\n", stderr);

}

void printf_error(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	fputs(COLOR_ERROR, stderr);
	vfprintf(stderr, format, args);
	fputs(COLOR_RESET "\n", stderr);

	va_end(args);

}
