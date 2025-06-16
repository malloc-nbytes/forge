#include <stdio.h>
#include <stdlib.h>

#include "graph.h"

adj_node *
adj_node_alloc(int pkg_i)
{
        adj_node *node = malloc(sizeof(adj_node));
        if (!node) {
                fprintf(stderr, "Memory allocation failed for adjacency node\n");
                exit(1);
        }
        node->pkg_i = pkg_i;
        node->next = NULL;
        return node;
}

dep_graph *
dep_graph_alloc(int pkgs_n)
{
        dep_graph *dg = malloc(sizeof(dep_graph));
        if (!dg) {
                fprintf(stderr, "Memory allocation failed for graph\n");
                exit(1);
        }
        dg->pkgs_n = pkgs_n;
        dg->ar = malloc(pkgs_n * sizeof(adj_list));
        if (!dg->ar) {
                fprintf(stderr, "Memory allocation failed for graph array\n");
                free(dg);
                exit(1);
        }
        for (int i = 0; i < pkgs_n; ++i) {
                dg->ar[i].hd = NULL;
        }
        return dg;
}

void
dep_graph_free(dep_graph *dg)
{
        if (!dg) return;
        for (int i = 0; i < dg->pkgs_n; ++i) {
                adj_node *node = dg->ar[i].hd;
                while (node) {
                        adj_node *tmp = node;
                        node = node->next;
                        free(tmp);
                }
        }
        free(dg->ar);
        free(dg);
}

void
add_edge(dep_graph *dg, int src, int dest)
{
        adj_node *node = adj_node_alloc(dest);
        node->next = dg->ar[src].hd;
        dg->ar[src].hd = node;
}

void
topological_sort_util(dep_graph *dg,
                      int        v,
                      int        visited[],
                      int        stack[],
                      int       *stack_index,
                      int        in_stack[])
{
        visited[v] = 1;
        in_stack[v] = 1;

        adj_node *node = dg->ar[v].hd;
        while (node) {
                int u = node->pkg_i;
                if (!visited[u]) {
                        topological_sort_util(dg, u, visited, stack, stack_index, in_stack);
                } else if (in_stack[u]) {
                        fprintf(stderr, "Cyclic dependency detected involving package index %d\n", u);
                        exit(1);
                }
                node = node->next;
        }

        in_stack[v] = 0;
        stack[(*stack_index)++] = v;
}

void
topological_sort(dep_graph *dg,
                 int        result[],
                 int       *result_size)
{
        int *visited = calloc(dg->pkgs_n, sizeof(int));
        int *in_stack = calloc(dg->pkgs_n, sizeof(int));
        int *stack = malloc(dg->pkgs_n * sizeof(int));
        int stack_index = 0;

        if (!visited || !in_stack || !stack) {
                fprintf(stderr, "Memory allocation failed for topological sort\n");
                exit(1);
        }

        for (int i = 0; i < dg->pkgs_n; ++i) {
                if (!visited[i]) {
                        topological_sort_util(dg, i, visited, stack, &stack_index, in_stack);
                }
        }

        *result_size = stack_index;
        for (int i = 0; i < stack_index; ++i) {
                result[i] = stack[stack_index - 1 - i];
        }

        free(visited);
        free(in_stack);
        free(stack);
}

