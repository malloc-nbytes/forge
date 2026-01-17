#ifndef FORGE_TRIE_H_INCLUDED
#define FORGE_TRIE_H_INCLUDED

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque type, use pointer for this structure.
typedef struct forge_trie forge_trie;

/**
 * Returns: a new prefix trie
 * Description: Allocate a new prefix trie. This datastructure
 *              can be used for autocompletions of strings.
 */
forge_trie *forge_trie_alloc(void);

/**
 * Parameter: t    -> the trie
 * Parameter: word -> a word to autocomplete
 * Returns: 1 on success, 0 on failure
 * Description: Insert a new word for autocompletion
 *              into the trie.
 */
int forge_trie_insert(forge_trie *t, const char *word);

/**
 * Parameter: t           -> the trie
 * Parameter: prefix      -> the prefix to lookup
 * Parameter: max_results -> max number of results returned
 * Parameter: out_count   -> the number of items found.
 * Returns: A string array of all autocompleted words. The length
 *          of this array is stored in `out_count'.
 * Description: Get an array of all words that can be completed
 *              from the prefix `prefix'.
 */
char **forge_trie_get_completions(forge_trie *t,
                                  const char *prefix,
                                  size_t      max_results,
                                  size_t     *out_count);

/**
 * Parameter: t -> the trie
 * Description: Free all memory alloc'd by the trie.
 */
void forge_trie_destroy(forge_trie *t);

#ifdef __cplusplus
}
#endif

#endif // FORGE_TRIE_H_INCLUDED
