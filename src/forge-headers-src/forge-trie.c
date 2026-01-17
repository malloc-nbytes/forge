#include <forge/array.h>
#include <stdlib.h>
#include <string.h>

typedef struct node node;

DYN_ARRAY_TYPE(node *, node_array);

struct node {
        char ch;
        int is_end_of_word;
        node_array children;
};

void *
forge_trie_alloc(void)
{
        node *root;
        if (!(root = malloc(sizeof(node))))
                return NULL;
        root->ch = '\0';
        root->is_end_of_word = 0;
        root->children = dyn_array_empty(node_array);
        return (void *)root;
}

static node *
node_alloc(char ch)
{
        node *n;
        if (!(n = malloc(sizeof(node))))
                return NULL;
        n->ch = ch;
        n->is_end_of_word = 0;
        n->children = dyn_array_empty(node_array);
        return n;
}

int
forge_trie_insert(void       *trie,
                  const char *word)
{
        if (!trie || !word)
                return 0;

        node   *root    = (node *)trie;
        node   *current = root;
        size_t  len     = strlen(word);

        for (size_t i = 0; i < len; ++i) {
                char ch = word[i];
                node *next = NULL;

                // Search for child with matching character
                for (size_t j = 0; j < current->children.len; ++j) {
                        node *child = current->children.data[j];
                        if (child->ch == ch) {
                                next = child;
                                break;
                        }
                }

                if (!next) {
                        next = node_alloc(ch);
                        if (!next)
                                return 0;
                        dyn_array_append(current->children, next);
                }
                current = next;

        }

        current->is_end_of_word = 1;
        return 1;
}
static node *
trie_find_prefix_node(void       *trie,
                      const char *prefix)
{
        // Find deepest node matching prefix

        if (!trie || !prefix)
                return NULL;

        node   *root    = (node *)trie;
        node   *current = root;
        size_t  len     = strlen(prefix);

        for (size_t i = 0; i < len; ++i) {
                char ch    = prefix[i];
                int  found = 0;
                for (size_t j = 0; j < current->children.len; ++j) {
                        node *child = current->children.data[j];
                        if (child->ch == ch) {
                                current = child;
                                found   = 1;
                                break;
                        }
                }

                if (!found)
                        return NULL;
        }

        return current;
}
static void
collect_words(node    *n,
              char    *buffer,
              size_t   buf_pos,
              size_t   buf_size,
              char   **results,
              size_t  *count,
              size_t   max_results)
{
        if (!n || *count >= max_results)
                return;

        if (n->is_end_of_word) {
                buffer[buf_pos] = '\0';
                if (*count < max_results) {
                        results[(*count)++] = strdup(buffer);
                }
        }

        for (size_t i = 0; i < n->children.len; ++i) {
                node *child = n->children.data[i];
                if (buf_pos + 1 >= buf_size) return;
                buffer[buf_pos] = child->ch;
                collect_words(child, buffer, buf_pos + 1, buf_size,
                              results, count, max_results);
        }
}
char **
forge_trie_get_completions(void       *trie,
                           const char *prefix,
                           size_t      max_results,
                           size_t     *out_count)
{
        *out_count = 0;

        if (!trie || !prefix)
                return NULL;

        node *start = trie_find_prefix_node(trie, prefix);
        if (!start)
                return NULL;

        char **results = calloc(max_results, sizeof(char *));
        if (!results)
                return NULL;

        char buffer[256];
        strncpy(buffer, prefix, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        size_t count = 0;
        collect_words(start, buffer, strlen(prefix), sizeof(buffer),
                      results, &count, max_results);

        *out_count = count;
        return results;
}
static void
trie_destroy_node(node *n)
{
        if (!n)
                return;

        for (size_t i = 0; i < n->children.len; ++i) {
                trie_destroy_node(n->children.data[i]);
        }

        dyn_array_free(n->children);
        free(n);
}
void
forge_trie_destroy(void *trie)
{
        if (!trie)
                return;

        trie_destroy_node((node *)trie);
}
