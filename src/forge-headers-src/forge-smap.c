/*
 * forge: Forge your system
 * Copyright (C) 2025  malloc-nbytes
 * Contact: zdhdev@yahoo.com

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "forge/smap.h"

// TODO: Resize table if load value is big enough.

static __forge_smap_node *
__forge_smap_node_alloc(const char *k, void *v)
{
        __forge_smap_node *n = (__forge_smap_node *)malloc(sizeof(__forge_smap_node));
        n->k = strdup(k);
        n->v = v;
        n->n = NULL;
        return n;
}

static unsigned
djb2(const char *s)
{
        unsigned hash = 5381;
        int c;

        while ((c = *s++))
                hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

        return hash;
}

forge_smap
forge_smap_create(void)
{
        return (forge_smap) {
                .tbl = (__forge_smap_node **)calloc(
                        FORGE_SMAP_DEFAULT_TBL_CAPACITY,
                        sizeof(__forge_smap_node *)
                ),
                .cap = FORGE_SMAP_DEFAULT_TBL_CAPACITY,
                .len = 0,
                .sz = 0,
        };
}

void
forge_smap_insert(forge_smap *map, const char *k, void *v)
{
        unsigned idx = djb2(k) % map->cap;
        __forge_smap_node *it = map->tbl[idx];
        __forge_smap_node *prev = NULL;

        while (it) {
                if (!strcmp(it->k, k)) {
                        it->v = v;
                        return;
                }
                prev = it;
                it = it->n;
        }

        it = __forge_smap_node_alloc(k, v);

        if (prev) {
                prev->n = it;
        } else {
                map->tbl[idx] = it;
                ++map->len;
        }

        ++map->sz;
}

int
forge_smap_contains(const forge_smap *map,
                    const char       *k)
{
        return forge_smap_get(map, k) != NULL;
}

void *
forge_smap_get(const forge_smap *map,
               const char       *k)
{
        unsigned idx = djb2(k) % map->cap;
        __forge_smap_node *it = map->tbl[idx];

        while (it) {
                if (!strcmp(it->k, k)) {
                        return it->v;
                }
                it = it->n;
        }

        return NULL;
}

void
forge_smap_destroy(forge_smap *map)
{
        for (size_t i = 0; i < map->cap; ++i) {
                __forge_smap_node *it = map->tbl[i];
                while (it) {
                        __forge_smap_node *tmp = it;
                        it = it->n;
                        free(tmp->k);
                        free(tmp);
                }
        }
        free(map->tbl);
        map->sz = map->len = map->cap = 0;
}

char **
forge_smap_iter(const forge_smap *map)
{
        char **keys = (char **)malloc(sizeof(char *) * (map->sz + 1));
        if (!keys) {
                return NULL;
        }

        size_t key_idx = 0;

        for (size_t i = 0; i < map->cap; ++i) {
                __forge_smap_node *node = map->tbl[i];
                while (node) {
                        keys[key_idx] = node->k;
                        key_idx++;
                        node = node->n;
                }
        }

        keys[key_idx] = NULL;

        return keys;
}

size_t
forge_smap_size(const forge_smap *map)
{
        return map->sz;
}
