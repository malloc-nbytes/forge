#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "depgraph.h"

depgraph_node *
depgraph_node_alloc(const char *name)
{
        depgraph_node *n = (depgraph_node *)malloc(sizeof(depgraph_node));
        n->name = strdup(name);
        n->next = NULL;
}

void
depgraph_node_free(depgraph_node *n)
{
        free(n->name);
        free(n);
        n = NULL;
}

depgraph
depgraph_create(void)
{
        return (depgraph) {
                .tbl = (depgraph_node **)calloc(
                        DEPGRAPH_DEFAULT_CAPACITY,
                        sizeof(depgraph_node *)
                ),
                .cap = DEPGRAPH_DEFAULT_CAPACITY,
                .len = 0,
                .sz = 0,
        };
}

void
depgraph_destroy(depgraph *dg)
{
        for (size_t i = 0; i < dg->len; ++i) {
                depgraph_node *it = dg->tbl[i]->next;
                while (it) {
                        depgraph_node *tmp = it->next;
                        depgraph_node_free(it);
                        it = tmp;
                }
                depgraph_node_free(dg->tbl[i]);
        }

        free(dg->tbl);
}

void
depgraph_insert_pkg(depgraph *dg, const char *name)
{
        // TODO: check for duplicates

        depgraph_node *n = depgraph_node_alloc(name);

        if (dg->len >= dg->cap) {
                dg->cap = !dg->cap ? 2 : dg->cap*2;
                dg->tbl = (depgraph_node **)realloc(
                        dg->tbl,
                        dg->cap * sizeof(depgraph_node *)
                );
                for (size_t i = dg->len; i < dg->cap; ++i) {
                        dg->tbl[i] = NULL;
                }
        }

        dg->tbl[dg->len++] = n;
}

void
depgraph_add_dep(depgraph   *dg,
                 const char *from,
                 const char *to)
{
        // TODO: check for not found
        // TODO: check for cyclic graph

        for (size_t i = 0; i < dg->len; ++i) {
                if (!strcmp(dg->tbl[i]->name, from)) {
                        depgraph_node *new_node = depgraph_node_alloc(to);
                        depgraph_node *tmp = dg->tbl[i]->next;
                        dg->tbl[i]->next = new_node;
                        new_node->next = tmp;
                        break;
                }
        }
}

void
depgraph_dump(const depgraph *dg)
{
        for (size_t i = 0; i < dg->len; ++i) {
                printf("Pkg: %s\n", dg->tbl[i]->name);
                depgraph_node *it = dg->tbl[i]->next;
                while (it) {
                        printf("  %s\n", it->name);
                        it = it->next;
                }
        }
}
