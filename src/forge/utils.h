#ifndef FORGE_UTILS_H_INCLUDED
#define FORGE_UTILS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parameter: pattern -> the regex pattern
 * Parameter: s       -> the string to test
 * Returns: 1 if match, 0 if no match
 * Description: Check if `s` regex matches `pattern`.
 */
int forge_utils_regex(const char *pattern, const char *s);

#ifdef __cplusplus
}
#endif

#endif // FORGE_UTILS_H_INCLUDED
