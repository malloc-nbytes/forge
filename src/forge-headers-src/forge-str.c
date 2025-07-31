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
#include "forge/array.h"

forge_str
forge_str_create(void)
{
        forge_str fs = (forge_str) {
                .data = NULL,
                .cap = 0,
                .len = 0,
        };
        return fs;
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

forge_str
forge_str_take(char *s)
{
        forge_str fs = forge_str_create();
        const size_t n = strlen(s);
        fs.data = s;
        fs.cap = n;
        fs.len = n;
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
                memset(fs->data + fs->len, 0, fs->cap - fs->len);
        }
        fs->data[fs->len++] = c;
        fs->data[fs->len] = '\0';
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

        // Edge case: str length less than substring length
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

void
forge_str_insert_at(forge_str *fs, char c, size_t idx)
{
        assert(fs != NULL);
        assert(idx <= fs->len);

        if (!fs->data || fs->len == 0) {
                forge_str_append(fs, c);
                return;
        }

        forge_str_append(fs, fs->data[fs->len-1]);

        for (size_t i = fs->len-2; i > idx; --i) {
                fs->data[i] = fs->data[i-1];
        }
        fs->data[idx] = c;
        fs->data[fs->len] = '\0';
}

char
forge_str_pop(forge_str *fs)
{
        char c = fs->data[fs->len-1];
        fs->data[--fs->len] = 0;
        return c;
}

char
forge_str_rm_at(forge_str *fs, size_t idx)
{
        assert(fs != NULL && fs->data != NULL);
        assert(idx < fs->len);

        char removed = fs->data[idx];
        for (size_t i = idx; i < fs->len-1; ++i) {
                fs->data[i] = fs->data[i+1];
        }
        fs->data[--fs->len] = '\0';
        return removed;
}

char **
forge_str_to_lines(const char *s, size_t *out_n)
{
        *out_n = 0;
        str_array lines = dyn_array_empty(str_array);
        forge_str buf = forge_str_create();

        for (size_t i = 0; s[i]; ++i) {
                if (s[i] == '\n') {
                        dyn_array_append(lines, strdup(buf.data));
                        forge_str_clear(&buf);
                } else {
                        forge_str_append(&buf, s[i]);
                }
        }

        if (buf.len > 0) {
                dyn_array_append(lines, strdup(buf.data));
        }
        forge_str_destroy(&buf);

        *out_n = lines.len;
        return lines.data;
}

char **
forge_str_take_to_lines(char *s, size_t *out_n)
{
        char **lines = forge_str_to_lines(s, out_n);
        free(s);
        return lines;
}
