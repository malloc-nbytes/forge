#include <string.h>
#include <stdio.h>

#include "forge/colors.h"
#include "forge/array.h"

static void
dyn_array_append_str(char_array *arr,
                     const char *str)
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

int
iskw(char *s)
{
        const char *kwds[] = {
                "void",
                "int",
                "const",
                "char",
                "float",
                "double",
                "size_t",
                "unsigned",
                "long",
                "typedef",
                "struct",
                "enum",
                "#define",
                "#ifndef",
                "#endif",
                "#if",
                "#else",
                "#endif",
                "#include",
                "__attribute__",
                "return",
                "break",
                "continue",
                "goto",
                "if",
                "else",
                "while",
                "for",
                "sizeof",
                "typeof",
        };

        for (size_t i = 0; i < sizeof(kwds)/sizeof(*kwds); ++i) {
                if (!strcmp(s, kwds[i])) {
                        return 1;
                }
        }
        return 0;
}

char *
forge_colors_c_to_string(const char *s)
{
        char_array buf = dyn_array_empty(char_array);
        char_array result = dyn_array_empty(char_array);
        static int in_multiline_comment = 0; // Persists across calls
        int in_single_line_comment = 0;
        int in_string = 0; // Track if inside a string literal
        int in_char = 0;   // Track if inside a character literal
        int escape = 0;    // Track escape sequences in strings/chars

        for (size_t i = 0; s[i]; ++i) {
                if (in_multiline_comment) {
                        // Inside a multiline comment, look for the end (*/)
                        if (s[i] == '*' && s[i + 1] == '/') {
                                dyn_array_append(buf, s[i]);
                                dyn_array_append(buf, s[i + 1]);
                                dyn_array_append(buf, 0);
                                dyn_array_append_str(&result, PINK BOLD);
                                dyn_array_append_str(&result, buf.data);
                                dyn_array_append_str(&result, RESET);
                                dyn_array_clear(buf);
                                in_multiline_comment = 0;
                                i++; // Skip the '/'
                                continue;
                        }
                        dyn_array_append(buf, s[i]);
                } else if (in_single_line_comment) {
                        // Inside a single-line comment, append until newline
                        if (s[i] == '\n') {
                                in_single_line_comment = 0;
                                dyn_array_append(buf, 0);
                                dyn_array_append_str(&result, PINK BOLD);
                                dyn_array_append_str(&result, buf.data);
                                dyn_array_append_str(&result, RESET);
                                dyn_array_clear(buf);
                                dyn_array_append(result, s[i]);
                        } else {
                                dyn_array_append(buf, s[i]);
                        }
                } else if (in_string) {
                        // Inside a string literal
                        dyn_array_append(buf, s[i]);
                        if (escape) {
                                escape = 0; // Reset escape flag after handling
                        } else if (s[i] == '\\') {
                                escape = 1; // Next character is escaped
                        } else if (s[i] == '"') {
                                // End of string literal
                                in_string = 0;
                                dyn_array_append(buf, 0);
                                dyn_array_append_str(&result, GREEN BOLD);
                                dyn_array_append_str(&result, buf.data);
                                dyn_array_append_str(&result, RESET);
                                dyn_array_clear(buf);
                        }
                } else if (in_char) {
                        // Inside a character literal
                        dyn_array_append(buf, s[i]);
                        if (escape) {
                                escape = 0; // Reset escape flag
                        } else if (s[i] == '\\') {
                                escape = 1; // Next character is escaped
                        } else if (s[i] == '\'') {
                                // End of character literal
                                in_char = 0;
                                dyn_array_append(buf, 0);
                                dyn_array_append_str(&result, GREEN BOLD);
                                dyn_array_append_str(&result, buf.data);
                                dyn_array_append_str(&result, RESET);
                                dyn_array_clear(buf);
                        }
                } else {
                        // Not in a comment, string, or char; check for starts or delimiters
                        if (s[i] == '/' && s[i + 1] == '*') {
                                dyn_array_append(buf, s[i]);
                                dyn_array_append(buf, s[i + 1]);
                                dyn_array_append(buf, 0);
                                dyn_array_append_str(&result, PINK BOLD);
                                dyn_array_append_str(&result, buf.data);
                                dyn_array_append_str(&result, RESET);
                                dyn_array_clear(buf);
                                in_multiline_comment = 1;
                                i++; // Skip the '*'
                                continue;
                        } else if (s[i] == '/' && s[i + 1] == '/') {
                                dyn_array_append(buf, s[i]);
                                dyn_array_append(buf, s[i + 1]);
                                in_single_line_comment = 1;
                                i++; // Skip the '/'
                                continue;
                        } else if (s[i] == '"') {
                                // Start of string literal
                                in_string = 1;
                                dyn_array_append(buf, s[i]);
                                continue;
                        } else if (s[i] == '\'') {
                                // Start of character literal
                                in_char = 1;
                                dyn_array_append(buf, s[i]);
                                continue;
                        } else if (s[i] == ';' || s[i] == '\n' || s[i] == '\t' ||
                                   s[i] == ' ' || s[i] == '(' || s[i] == ')' || s[i] == ',') {
                                // Handle delimiters
                                dyn_array_append(buf, 0);
                                if (buf.len > 0 && iskw(buf.data)) {
                                        dyn_array_append_str(&result, YELLOW BOLD);
                                        dyn_array_append_str(&result, buf.data);
                                        dyn_array_append_str(&result, RESET);
                                } else if (buf.len > 0) {
                                        dyn_array_append_str(&result, buf.data);
                                }
                                dyn_array_clear(buf);
                                dyn_array_append(result, s[i]);
                        } else {
                                dyn_array_append(buf, s[i]);
                        }
                }
        }

        dyn_array_append(buf, 0);
        if (buf.len > 0) {
                if (in_multiline_comment || in_single_line_comment) {
                        dyn_array_append_str(&result, PINK BOLD);
                } else if (in_string || in_char) {
                        dyn_array_append_str(&result, GREEN BOLD);
                } else if (iskw(buf.data)) {
                        dyn_array_append_str(&result, YELLOW BOLD);
                }
                dyn_array_append_str(&result, buf.data);
                dyn_array_append_str(&result, RESET);
        }

        dyn_array_append(result, 0);
        char *final_string = strdup(result.data);

        dyn_array_free(buf);
        dyn_array_free(result);

        return final_string;
}
