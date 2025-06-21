#ifndef SMAP_H_INCLUDED
#define SMAP_H_INCLUDED

#include <stddef.h>

#define FORGE_SMAP_DEFAULT_TBL_CAPACITY 2048

typedef struct __forge_smap_node {
        char *k; // owns the string
        void *v;
        struct __forge_smap_node *n;
} __forge_smap_node;

typedef struct {
        __forge_smap_node **tbl;
        size_t len; // number of table entries
        size_t cap; // capacity of table
        size_t sz; // how many total nodes
} forge_smap;

forge_smap forge_smap_create(void);
void forge_smap_insert(forge_smap *map, const char *k, void *v);
int forge_smap_contains(const forge_smap *map, const char *k);
void *forge_smap_get(const forge_smap *map, const char *k);
void forge_smap_destroy(forge_smap *map);
char **smap_iter(const forge_smap *map);
size_t forge_smap_size(const forge_smap *map);

#endif // SMAP_H_INCLUDED
