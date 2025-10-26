/*
 * forge: Forge your own packages
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

#ifndef DEPGRAPH_H_INCLUDED
#define DEPGRAPH_H_INCLUDED

#include <stddef.h>

#include "forge/array.h"

#define DEPGRAPH_DEFAULT_CAPACITY 256

typedef struct depgraph_node {
        char *name;
        struct depgraph_node *next;
} depgraph_node;

// Does not take ownership of `name`.
depgraph_node *depgraph_node_alloc(const char *name);
void depgraph_node_free(depgraph_node *n);

typedef struct {
        depgraph_node **tbl; // adjacency list of nodes
        size_t len;          // number of tbl slots used
        size_t cap;          // total number of tbl slots used
        size_t sz;           // number of nodes
} depgraph;

depgraph depgraph_create(void);
void depgraph_destroy(depgraph *dg);

// Does not take ownership of `name`.
void depgraph_insert_pkg(depgraph *dg, const char *name);

// Does not take ownership of `from` and `to`.
void depgraph_add_dep(depgraph *dg, const char *from, const char *to);
size_t_array depgraph_gen_order(const depgraph *dg);
void depgraph_dump(const depgraph *dg);

#endif // DEPGRAPH_H_INCLUDED
