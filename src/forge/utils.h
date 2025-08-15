#ifndef FORGE_UTILS_H_INCLUDED
#define FORGE_UTILS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define _NOOP_1(x) ((void)(x))
#define _NOOP_2(x, y) ((void)(x), (void)(y))
#define _NOOP_3(x, y, z) ((void)(x), (void)(y), (void)(z))
#define _NOOP_4(x, y, z, w) ((void)(x), (void)(y), (void)(z), (void)(w))
#define _NOOP_5(x, y, z, w, v) ((void)(x), (void)(y), (void)(z), (void)(w), (void)(v))
#define _NOOP_6(x, y, z, w, v, u) ((void)(x), (void)(y), (void)(z), (void)(w), (void)(v), (void)(u))
#define _NOOP_7(x, y, z, w, v, u, t) ((void)(x), (void)(y), (void)(z), (void)(w), (void)(v), (void)(u), (void)(t))
#define _NOOP_8(x, y, z, w, v, u, t, s) ((void)(x), (void)(y), (void)(z), (void)(w), (void)(v), (void)(u), (void)(t), (void)(s))

#define _GET_NOOP_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME

// Declare something as no operation to silence warnings.
#define NOOP(...) _GET_NOOP_MACRO(__VA_ARGS__, _NOOP_8, _NOOP_7, _NOOP_6, _NOOP_5, _NOOP_4, _NOOP_3, _NOOP_2, _NOOP_1)(__VA_ARGS__)

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
