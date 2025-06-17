#ifndef DEPGRAPH_H_INCLUDED
#define DEPGRAPH_H_INCLUDED

#include <stddef.h>

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

// Does not take ownership of `from` and `todo`.
void depgraph_add_dep(depgraph *dg, const char *from, const char *to);
void depgraph_dump(const depgraph *dg);

#endif // DEPGRAPH_H_INCLUDED
