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
void print_error(const char *errstr, int errnum);
void printf_error(const char *format, ...);

#endif
