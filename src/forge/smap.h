#ifndef FORGE_SMAP_H_INCLUDED
#define FORGE_SMAP_H_INCLUDED

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FORGE_SMAP_DEFAULT_TBL_CAPACITY 2048

typedef struct __forge_smap_node {
        char *k; // owns the string
        void *v;
        struct __forge_smap_node *n;
} __forge_smap_node;

// A string map provided by forge
typedef struct {
        __forge_smap_node **tbl;
        size_t len; // number of table entries
        size_t cap; // capacity of table
        size_t sz; // how many total nodes
} forge_smap;

/**
 * Returns: a new string map
 * Description: Create a new string map. Make sure to
 *              call forge_smap_destroy() to free memory.
 */
forge_smap forge_smap_create(void);

/**
 * Parameter: map -> the map to insert to
 * Parameter: k   -> the key
 * Parameter: v   -> the value
 * Description: Insert key `k` with value `v` into the map `map`.
 */
void forge_smap_insert(forge_smap *map, const char *k, void *v);

/**
 * Parameter: map -> the map to query
 * Parameter: k   -> the key to check
 * Returns: 1 if found, 0 if otherwise
 * Description: See if the key `k` is inside of the map `map`.
 */
int forge_smap_contains(const forge_smap *map, const char *k);

/**
 * Parameter: map -> the map to get from
 * Parameter: k   -> the key with the associated value
 * Returns: the value of of the key `k`.
 * Description: Get the value that the key `k` is associated with.
 */
void *forge_smap_get(const forge_smap *map, const char *k);

/**
 * Parameter: map -> the map to destroy
 * Description: free()'s all memory that `map` allocates.
 */
void forge_smap_destroy(forge_smap *map);

/**
 * Parameter: map -> the map to iterate
 * Returns: an array of keys (NULL terminated, NULL on failure)
 * Description: Use this function to get all keys inside of
 *              the map. It is guaranteed to be NULL terminated.
 *              The result needs to be free()'d. The individual strings
 *              do not need to be free()'d.
 */
char **forge_smap_iter(const forge_smap *map);

/**
 * Paramter: map -> the map to get the size from
 * Returns: the number of nodes in the map
 * Description: Get the number of nodes stored inside of `map`.
 */
size_t forge_smap_size(const forge_smap *map);

#ifdef __cplusplus
}
#endif

#endif // FORGE_SMAP_H_INCLUDED
