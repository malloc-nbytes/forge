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

#include "colors.h"

#include <forge/array.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>

void
dyn_array_append_str(char_array *arr, const char *str)
{
        if (!arr || !str) {
                return; // Handle null inputs
        }

        // Calculate length of the input string (excluding null terminator)
        size_t str_len = strlen(str);

        // Ensure the array has enough capacity
        while (arr->len + str_len >= arr->cap) {
                // Double the capacity or set to a minimum if zero
                size_t new_cap = arr->cap == 0 ? 16 : arr->cap * 2;
                char *new_data = realloc(arr->data, new_cap * sizeof(char));
                if (!new_data) {
                        fprintf(stderr, "Failed to reallocate memory for char_array\n");
                        return; // Memory allocation failure
                }
                arr->data = new_data;
                arr->cap = new_cap;
        }

        // Copy the string into the array
        memcpy(arr->data + arr->len, str, str_len);
        arr->len += str_len;
}

void
good_major(const char *msg, int newline)
{
        printf(GREEN BOLD "\n*** %s" RESET, msg);
        if (newline) { printf("\n\n"); }
        sleep(1);
}

void
good_minor(const char *msg, int newline)
{
        printf(GREEN "%s" RESET, msg);
        if (newline) { putchar('\n'); }
}

void
info_major(const char *msg, int newline)
{
        printf(YELLOW BOLD "*** %s" RESET, msg);
        if (newline) { putchar('\n'); }
        sleep(1);
}

void
info_minor(const char *msg, int newline)
{
        printf(YELLOW "%s" RESET, msg);
        if (newline) { putchar('\n'); }
}

void
bad_major(const char *msg, int newline)
{
        printf(RED BOLD "*** %s" RESET, msg);
        if (newline) { putchar('\n'); }
        sleep(1);
}

void
bad_minor(const char *msg, int newline)
{
        printf(RED "%s" RESET, msg);
        if (newline) { putchar('\n'); }
}
