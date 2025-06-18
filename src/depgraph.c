#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "depgraph.h"
#include "dyn_array.h"
#include "ds/array.h"

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

static ssize_t
get_index_of_pkg(const depgraph *dg,
                 const char     *name)
{
        for (size_t i = 0; i < dg->len; ++i) {
                if (!strcmp(dg->tbl[i]->name, name)) {
                        return i;
                }
        }

        return -1;
}

static void
__depgraph_gen_order(const depgraph *dg, size_t_array *ar, size_t st, int *visited, int *rec_stack)
{
        if (rec_stack[st]) {
                // Cycle detected, but we can skip for topological sort purposes
                // Optionally, you can handle this differently (report an error)
                return;
        }

        if (visited[st]) {
                // Node already processed
                return;
        }

        // Mark node as part of the current recursion stack
        rec_stack[st] = 1;

        // Process all dependencies
        depgraph_node *it = dg->tbl[st]->next;
        while (it) {
                ssize_t index = get_index_of_pkg(dg, it->name);
                if (index == -1) {
                        assert(0 && "Package not found in graph");
                }
                __depgraph_gen_order(dg, ar, (size_t)index, visited, rec_stack);
                it = it->next;
        }

        // Mark node as visited and remove from recursion stack
        visited[st] = 1;
        rec_stack[st] = 0;

        // Append the current node to the result (post-order)
        dyn_array_append(*ar, st);
}

size_t_array
depgraph_gen_order(const depgraph *dg)
{
        size_t_array ar = dyn_array_empty(size_t_array);

        // Initialize visited and recursion stack arrays
        int *visited = (int *)calloc(dg->len, sizeof(int));
        int *rec_stack = (int *)calloc(dg->len, sizeof(int));

        // Process all nodes to handle disconnected components
        for (size_t i = 0; i < dg->len; ++i) {
                if (!visited[i]) {
                        __depgraph_gen_order(dg, &ar, i, visited, rec_stack);
                }
        }

        // Free temporary arrays
        free(visited);
        free(rec_stack);

        return ar;
}

/* static void */
/* __depgraph_gen_order(const depgraph *dg, */
/*                      size_t_array   *ar, */
/*                      size_t          st) */
/* { */
/*         // TODO: detect cyclic order */
/*         // TODO: use a hashset for track visited. */
/*         int found = 0; */
/*         for (size_t j = 0; j < ar->len; ++j) { */
/*                 for (size_t k = j; k < ar->len-1; ++k) { */
/*                         if (ar->data[j] == ar->data[k]) { */
/*                                 found = 1; */
/*                                 break; */
/*                         } */
/*                 } */
/*                 if (found) { break; } */
/*         } */
/*         if (found) { return; } */

/*         depgraph_node *it = dg->tbl[st]->next; */

/*         while (it) { */
/*                 ssize_t index = get_index_of_pkg(dg, it->name); */
/*                 if (index == -1) { */
/*                         assert(0 && "something went horribly wrong"); */
/*                 } */
/*                 __depgraph_gen_order(dg, ar, (size_t)index); */
/*                 it = it->next; */
/*         } */

/*         printf("adding: %zu\n", st); */
/*         dyn_array_append(*ar, st); */
/* } */

/* size_t_array */
/* depgraph_gen_order(const depgraph *dg) */
/* { */
/*         size_t_array ar = dyn_array_empty(size_t_array); */
/*         for (size_t i = 0; i < dg->len; ++i) { */
/*                 __depgraph_gen_order(dg, &ar, i); */
/*         } */
/*         return ar; */
/* } */

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
