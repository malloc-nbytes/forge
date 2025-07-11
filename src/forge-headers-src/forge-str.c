/*
 * forge: Forge your own packages
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "forge/str.h"

forge_str
forge_str_create(void)
{
        return (forge_str) {
                .data = NULL,
                .cap = 0,
                .len = 0,
        };
}

forge_str
forge_str_from(const char *s)
{
        forge_str fs = forge_str_create();
        for (size_t i = 0; s[i]; ++i) {
                forge_str_append(&fs, s[i]);
        }
        return fs;
}

void
forge_str_clear(forge_str *fs)
{
        fs->len = 0;
        memset(fs->data, 0, fs->cap);
}

void
forge_str_destroy(forge_str *fs)
{
        if (fs->data) {
                free(fs->data);
        }
        fs->len = fs->cap = 0;
}

void
forge_str_append(forge_str *fs, char c)
{
        if (fs->len >= fs->cap) {
                fs->cap = fs->cap ? fs->cap * 2 : 2;
                fs->data = (char *)realloc(fs->data, fs->cap);
                (void)memset(fs->data+fs->len, 0, fs->cap - fs->len);
        }
        fs->data[fs->len++] = c;
}

void
forge_str_concat(forge_str  *fs,
                 const char *s)
{
        for (size_t i = 0; s[i]; ++i) {
                forge_str_append(fs, s[i]);
        }
}

int
forge_str_eq(const forge_str *s0,
             const forge_str *s1)
{
        return !strcmp(s0->data, s1->data);
}

int
forge_str_eq_cstr(const forge_str *s0,
                  const char      *s1)
{
        return !strcmp(s0->data, s1);
}

char *
forge_str_to_cstr(const forge_str *fs)
{
        return fs->data;
}

char *
forge_str_contains_substr(const forge_str *fs,
                          const char      *substr,
                          int              case_sensitive)
{
        size_t substr_n = strlen(substr);

        // Edge case: non-empty substring, empty forge_str
        if (fs->len == 0 && substr_n != 0) {
                return NULL;
        }

        // Edge case: empty substring
        if (substr_n == 0) {
                return fs->data;
        }

        // Edge case: forge_str length less than substring length
        if (fs->len < substr_n) {
                return NULL;
        }

        if (case_sensitive) {
                for (size_t i = 0; i <= fs->len - substr_n; i++) {
                        if (strncmp(fs->data + i, substr, substr_n) == 0) {
                                return fs->data + i;
                        }
                }
        } else {
                for (size_t i = 0; i <= fs->len - substr_n; i++) {
                        int match = 1;
                        for (size_t j = 0; j < substr_n; j++) {
                                if (tolower((unsigned char)(fs->data[i + j])) !=
                                    tolower((unsigned char)substr[j])) {
                                        match = 0;
                                        break;
                                }
                        }
                        if (match) {
                                return fs->data + i;
                        }
                }
        }

        return NULL;
}

char *
forge_str_builder(const char *first, ...)
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
