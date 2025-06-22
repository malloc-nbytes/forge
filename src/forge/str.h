#ifndef STR_H_INCLUDED
#define STR_H_INCLUDED

typedef struct {
        char *data;
        size_t len, cap;
} forge_str;

forge_str forge_str_create(void);
forge_str forge_str_from(const char *s);
void forge_str_destroy(forge_str *fs);
void forge_str_append(forge_str *fs, char c);
void forge_str_concat(forge_str *fs, const char *s);
int forge_str_eq(const forge_str *s0, const forge_str *s1);
int forge_str_eq_cstr(const forge_str *s0, const char *s1);
char *forge_str_to_cstr(const forge_str *fs);
char *forge_str_contains_substr(
        const forge_str *fs,
        const char *substr,
        int case_sensitive
);
#endif // STR_H_INCLUDED
