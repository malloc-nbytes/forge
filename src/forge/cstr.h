#ifndef FORGE_CSTR_H_INCLUDED
#define FORGE_CSTR_H_INCLUDED

/**
 * Parameter: first -> the first string
 * VARIADIC         -> other strings
 * Returns: the concatination of all strings
 * Description: Build a string of the variadic parameters.
 *              Note: Remember to put NULL as the last argument!
 */
char *forge_cstr_builder(const char *first, ...);

#endif // FORGE_CSTR_H_INCLUDED
