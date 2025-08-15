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
