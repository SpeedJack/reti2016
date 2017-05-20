#ifndef	_BATTLE_CONSOLE_H
#define	_BATTLE_CONSOLE_H

#include <stdint.h>

bool string_to_uint16(const char *str, uint16_t *dst);
int flush_stdin();
int get_line(char *buffer, size_t size);
bool get_uint16(uint16_t *result);
char get_character();
char *trim_white_spaces(char *str);
char *split_cmd_args(char *cmd);
/*bool starts_case_with(const char *str, const char *prefix);*/
void print_error(const char *errstr, int errnum);
void printf_error(const char *format, ...);

#endif
