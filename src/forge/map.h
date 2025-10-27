#ifndef FORGE_MAP_H_INCLUDED
#define FORGE_MAP_H_INCLUDED

#include <stddef.h>
#include <stdlib.h>

#define FORGE_MAP_DEFAULT_CAPACITY 2048

#define FORGE_MAP_TYPE(ktype, vtype, mapname) \
        typedef unsigned (*forge_##mapname##_hash_sig)(ktype *); \
        typedef int      (*forge_##mapname##_cmp_sig)(ktype *, ktype *); \
        \
        typedef struct __##mapname##_node { \
                ktype k; \
                vtype v; \
                struct __##mapname##_node *n; \
        } __##mapname##_node; \
        \
        typedef struct { \
                struct { \
                        __##mapname##_node **data; \
                        size_t len; \
                        size_t cap; \
                        size_t sz; \
                } tbl; \
                forge_##mapname##_hash_sig hash; \
                forge_##mapname##_cmp_sig cmp; \
        } mapname; \
        \
        mapname mapname##_create(forge_##mapname##_hash_sig hash, forge_##mapname##_cmp_sig cmp); \
        void mapname##_destroy(mapname *map); \
        void mapname##_insert(mapname *map, ktype k, vtype v); \
        int mapname##_contains(mapname *map, ktype k); \
        vtype *mapname##_get(mapname *map, ktype k); \
        \
        mapname \
        mapname##_create(forge_##mapname##_hash_sig hash, \
                         forge_##mapname##_cmp_sig cmp) \
        { \
                __##mapname##_node **data \
                        = (__##mapname##_node **)calloc(FORGE_MAP_DEFAULT_CAPACITY, sizeof(__##mapname##_node *)); \
                return (mapname) { \
                        .tbl = { \
                                .data = data, \
                                .len = 0, \
                                .cap = FORGE_MAP_DEFAULT_CAPACITY, \
                                .sz = 0, \
                        }, \
                        .hash = hash, \
                        .cmp = cmp, \
                }; \
        } \
        void \
        mapname##_insert(mapname *map, ktype k, vtype v) \
        { \
                unsigned idx = map->hash(&k) % map->tbl.cap; \
                __##mapname##_node *it = map->tbl.data[idx]; \
                __##mapname##_node *prev = NULL; \
                while (it) { \
                        if (!map->cmp(&it->k, &k)) { \
                                it->v = v; \
                                return; \
                        } \
                        prev = it; \
                        it = it->n; \
                } \
                it = (__##mapname##_node *)malloc(sizeof(__##mapname##_node)); \
                it->k = k; \
                it->v = v; \
                it->n = NULL; \
                if (prev) { \
                        prev->n = it; \
                } else { \
                        map->tbl.data[idx] = it; \
                        ++map->tbl.len; \
                } \
                ++map->tbl.sz; \
        } \
        \
        int \
        mapname##_contains(mapname *map, ktype k) \
        { \
                return mapname##_get(map, k) != NULL; \
        } \
        vtype * \
        mapname##_get(mapname *map, ktype k) \
        { \
                unsigned idx = map->hash(&k) % map->tbl.cap; \
                __##mapname##_node *it = map->tbl.data[idx]; \
                while (it) { \
                        if (!map->cmp(&it->k, &k)) { \
                                return &it->v; \
                        } \
                        it = it->n; \
                } \
                return NULL; \
        } \

#endif // FORGE_MAP_H_INCLUDED
