#ifndef FORGE_CSTR_H_INCLUDED
#define FORGE_CSTR_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parameter: first -> the first string
 * VARIADIC         -> other strings
 * Returns: the concatination of all strings
 * Description: Build a string of the variadic parameters.
 *              Note: Remember to put NULL as the last argument!
 */
char *forge_cstr_builder(const char *first, ...);

/**
 * Parameter: s -> the string to look through
 * Parameter: c -> the character to find
 * Returns: a pointer to the first occurrence of char `c` or NULL if not found.
 * Description: Look through the string `s` for the first occurrence of `c`.
 *              If it is found, a pointer to it is returned, otherwise NULL is returned.
 */
const char *forge_cstr_first_of(const char *s, char c);

/**
 * Parameter: s -> the string to look through
 * Parameter: c -> the character to find
 * Returns: a pointer to the last occurrence of char `c` or NULL if not found.
 * Description: Look through the string `s` for the last occurrence of `c`.
 *              If it is found, a pointer to it is returned, otherwise NULL is returned.
 */
const char *forge_cstr_last_of(const char *s, char c);

/**
 * Parameter: i -> the integer to convert
 * Returns: a string of `i`
 * Description: Convert integer `i` to a string.
 */
int forge_cstr_of_int(int i);

#ifdef __cplusplus
}
#endif

#endif // FORGE_CSTR_H_INCLUDED
