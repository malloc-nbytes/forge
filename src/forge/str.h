#ifndef FORGE_STR_H_INCLUDED
#define FORGE_STR_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
        char *data;
        size_t len, cap;
} forge_str;

/**
 * Returns: an empty forge_str
 * Description: Create a new empty forge_str.
 */
forge_str forge_str_create(void);

/**
 * Parameter: s -> the string to create from
 * Returns: a new forge_str created from `s`.
 * Description: Creates a new forge_str with the
 *              content of `s`.
 */
forge_str forge_str_from(const char *s);

/**
 * Parameter: s -> the string to take from
 * Returns: a new forge_str created from `s`.
 * Description: Creates a new forge_str with the
 *              content of `s`. It will take ownership
 *              of the pointer so it will be destroyed
 *              in `forge_str_destroy()`.
 */
forge_str forge_str_take(char *s);

/**
 * Parameter: fs -> the forge_str
 * Description: Clear the string `fs`.
 */
void forge_str_clear(forge_str *fs);

/**
 * Parameter: fs -> the forge_str
 * Description: free() all memory associated with `fs`. This also
                resets all other members of `fs` so it can be re-used.
 */
void forge_str_destroy(forge_str *fs);

/**
 * Parameter: fs -> the forge_str to append to
 * Parameter: c  -> the character to append
 * Description: Append the character `c` to `fs`.
 */
void forge_str_append(forge_str *fs, char c);

/**
 * Parameter: fs -> the forge_str to concat to
 * Parameter: s -> the string to concat
 * Description: Concatinate `s` to `fs`.
 */
void forge_str_concat(forge_str *fs, const char *s);

/**
 * Paramter: s0 -> the first forge_str
 * Paramter: s1 -> the second forge_str
 * Returns: 1 if they are equal, and 0 if otherwise
 * Description: Check if s0 == s1.
 */
int forge_str_eq(const forge_str *s0, const forge_str *s1);

/**
 * Parameter: s0 -> the forge_str
 * Parameter: s1 -> the c_str
 * Returns: 1 if they are equal, and 0 if otherwise
 * Description: Checks if s0.data == s1.
 */
int forge_str_eq_cstr(const forge_str *s0, const char *s1);

/**
 * Parameter: fs -> the forge_str
 * Returns: the c_str of `fs`
 * Description: Get the underlying c_str data of `fs`.
 */
char *forge_str_to_cstr(const forge_str *fs);

/**
 * Parameter: fs             -> the forge_str to search in
 * Parameter: substr         -> the substring to search
 * Parameter: case_sensitive -> whether it should be case sensitive
 * Returns: a pointer to the start of the substring if found,
 *          or NULL if not found
 * Description: Check `fs` for substring `substr`.
 */
char *forge_str_contains_substr(
        const forge_str *fs,
        const char *substr,
        int case_sensitive
);

/**
 * Parameter: fs  -> the forge_str to insert into
 * Parameter: c   -> the character to insert
 * Parameter: idx -> the index to insert at
 * Description: Insert character `c` into string `fs` at index `idx`.
 */
void forge_str_insert_at(forge_str *fs, char c, size_t idx);

/**
 * Parameter: first -> the first string
 * VARIADIC         -> other strings
 * Returns: the concatination of all strings
 * Description: Build a string of the variadic parameters.
 *              Note: Remember to put NULL as the last argument!
 */
char *forge_str_builder(const char *first, ...);

/**
 * Parameter: fs -> the forge_string
 * Returns: the character that was removed
 * Description: Pop's the last character off of
 *              the string `fs`. It is up to you
 *              to make sure that `fs.len > 0`.
 */
char forge_str_pop(forge_str *fs);

/**
 * Parameter: fs  -> the forge_str
 * Parameter: idx -> the index of the character to remove
 * Returns: the deleted character
 * Description: Remove the character at `idx` in `fs`.
 */
char forge_str_rm_at(forge_str *fs, size_t idx);

#ifdef __cplusplus
}
#endif

#endif // FORGE_STR_H_INCLUDED
