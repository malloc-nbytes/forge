/*
 * forge: Forge your system
 * Copyright (C) 2025  malloc-nbytes
 * Contact: zdhdev@yahoo.com

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include "forge/cstr.h"

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

char *
forge_cstr_builder(const char *first, ...)
{
        va_list args;
        size_t total_length = 0;

        // Get total length
        va_start(args, first);
        const char *str = first;
        while (str != NULL) {
                total_length += strlen(str);
                str = va_arg(args, const char*);
        }
        va_end(args);

        char *result = (char *)malloc(total_length + 1);
        if (result == NULL) return NULL;

        size_t pos = 0;
        va_start(args, first);
        str = first;
        while (str != NULL) {
                strcpy(result + pos, str);
                pos += strlen(str);
                str = va_arg(args, const char*);
        }
        va_end(args);

        result[total_length] = '\0';
        return result;
}

const char *
forge_cstr_first_of(const char *s, char c)
{
        if (!s) return NULL;

        for (size_t i = 0; s[i]; ++i) {
                if (s[i] == c) return &s[i];
        }

        return NULL;
}

const char *
forge_cstr_last_of(const char *s, char c)
{
        if (!s) return NULL;

        const char *last = NULL;

        for (size_t i = 0; s[i]; ++i) {
                if (s[i] == c) last = &s[i];
        }

        return last;
}

char *
forge_cstr_of_int(int i)
{
        int digits = 0;
        if (i < 10)         digits = 1;
        if (i < 100)        digits = 2;
        if (i < 1000)       digits = 3;
        if (i < 10000)      digits = 4;
        if (i < 100000)     digits = 5;
        if (i < 1000000)    digits = 6;
        if (i < 10000000)   digits = 7;
        if (i < 100000000)  digits = 8;
        if (i < 1000000000) digits = 9;
        else digits = 10;

        char *s = (char *)malloc(digits + 1);
        sprintf(s, "%d", i);
        s[digits-1] = 0;
        return s;
}
