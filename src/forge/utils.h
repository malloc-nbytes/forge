#ifndef FORGE_UTILS_H_INCLUDED
#define FORGE_UTILS_H_INCLUDED

/**
 * Parameter: st  -> the start
 * Parameter: end -> the end
 * Returns: an integer in the range [st, end]
 * Description: Generates a random integer from [st, end].
 *              If end > st, then it will swap(st, end).
 */
int forge_utils_rand_in_range(int st, int end);

#endif // FORGE_UTILS_H_INCLUDED
