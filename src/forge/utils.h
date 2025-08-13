#ifndef FORGE_UTILS_H_INCLUDED
#define FORGE_UTILS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

// Perform a foreach loop on each element in `ar` with
// the named value of `k` until `len`, doing `blk`.
#define FOREACH(k, ar, len, blk)                                \
        for (size_t __iter = 0; __iter < len; ++__iter) {       \
                typeof((ar)[__iter]) k = (ar)[__iter];          \
                blk;                                            \
        }

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
