#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

char *
forge_str_to_cstr(const forge_str *fs)
{
        return fs->data;
}

int
forge_str_contains_substr(const forge_str *fs,
                          const char      *substr,
                          int              case_sensitive)
{
        size_t substr_n = strlen(substr);

        // Edge case: non-empty substring, empty forge_str
        if (fs->len == 0 && substr_n != 0) {
                return 0;
        }

        // Edge case: empty substring
        if (substr_n == 0) {
                return 1;
        }

        // Edge case: forge_str length less than substring length
        if (fs->len < substr_n) {
                return 0;
        }

        if (case_sensitive) {
                for (size_t i = 0; i <= fs->len - substr_n; i++) {
                        if (strncmp(fs->data + i, substr, substr_n) == 0) {
                                return 1;
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
                                return 1;
                        }
                }
        }

        return 0;
}
