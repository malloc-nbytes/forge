#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "forge/array.h"

#include "utils.h"

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
