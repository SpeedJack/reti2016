#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "console.h"

/*
 * Converts the null-terminated string pointed by src to a uint16_t and saves
 * the result in the memory area pointed by dst.
 * The return value is true on successful conversion, otherwise false. In
 * addiction, conversion is considered failed if there's something not-a-digit
 * in the source string.
 */
bool string_to_uint16(const char *str, uint16_t *dst)
{
	char *garbage = NULL;
	long value = 0;

	errno = 0;
	value = strtol(str, &garbage, 0);

	if (errno || (garbage && *garbage != '\0') ||
			value < 0 || value > (1 << 16) - 1)
		return false;

	*dst = (uint16_t)value;
	return true;
}

/*
 * Flushes the stdin buffer reading all characters remaining in it, and returns
 * the total number of characters read. The caller must be sure that something
 * is in the buffer, or the function will stuck on getchar() call.
 */
int flush_stdin()
{
	char c;
	int len;

	len = 0;
	while ((c = getchar()) != '\n' && c != EOF)
		len++;
	return len;
}

/*
 * Gets a line from stdin without the terminating newline, saves it in the
 * memory area pointed by buffer and returns the total number of characters
 * typed (may be greater than size). If more than size characters are read, the
 * input is truncated and the rest of stdin is flushed. The function guarantees
 * that the resulting string is always null-terminated.
 */
int get_line(char *buffer, size_t size)
{
	int len;

	assert(size > 0);

	fgets(buffer, size, stdin);

	len = strlen(buffer);
	if (buffer[len-1] == '\n')
		buffer[len-1] = '\0';
	else
		len += flush_stdin();
	return len - 1;
}

/*
 * Reads a number from stdin and places the result in the memory area pointed
 * by result. The return value is true if the input has been successfully read
 * and converted. Otherwise false.
 */
bool get_uint16(uint16_t *result)
{
	char buffer[UINT16_BUFFER_SIZE];

	if (get_line(buffer, UINT16_BUFFER_SIZE) >= UINT16_BUFFER_SIZE)
		return false;
	return string_to_uint16(buffer, result);
}

/*
 * Reads a non-space character from stdin and flushes the rest of the buffer.
 */
char get_character()
{
	int ch;

	do {
		ch = getchar();
		if ((char)ch == '\n' || ch == EOF)
			return (char)ch;
	} while (isspace(ch));

	flush_stdin();

	return (char)ch;
}

/*
 * Returns a pointer to the first non-space character of the null-terminated
 * string pointed by str and removes any trailing space from the string.
 */
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

/*
 * Splits a command from his list of argument (separated by space) and trim
 * any white space surrounding the argument list. Returns a pointer to the
 * start of the first argument.
 */
char *split_cmd_args(char *cmd)
{
	while (*cmd && !isspace((unsigned char)*cmd)) cmd++;
	if (*cmd == '\0')
		return NULL;
	*cmd = '\0';

	return trim_white_spaces(cmd + 1);
}

/*
 * Prints an error. If errnum is specified, a description of the error code
 * is also printed.
 */
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

/*
 * Formats and prints an error.
 */
void printf_error(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	fputs(COLOR_ERROR, stderr);
	vfprintf(stderr, format, args);
	fputs(COLOR_RESET "\n", stderr);

	va_end(args);

}
