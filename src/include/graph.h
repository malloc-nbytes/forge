#ifndef GRAPH_H_INCLUDED
#define GRAPH_H_INCLUDED

#include "cpm.h"

typedef struct {
        pkg *pkg;
        void *handle; // dlopen handle
        char *path;   // xxx.so path
} pkg_info;

typedef struct adj_node {
        int pkg_i; // Index in pkg array
        struct adj_node *next;
} adj_node;

typedef struct {
        adj_node *hd;
} adj_list;

typedef struct {
        adj_list *ar;
        int pkgs_n;
} dep_graph;

adj_node *adj_node_alloc(int pkg_i);
dep_graph *dep_graph_alloc(int pkgs_n);
void dep_graph_free(dep_graph *dg);
void add_edge(dep_graph *dg, int src, int dest);
void topological_sort_util(
        dep_graph *dg,
        int v,
        int visited[],
        int stack[],
        int *stack_index,
        int in_stack[]
);
void topological_sort(
        dep_graph *dg,
        int result[],
        int *result_size
);

#endif // GRAPH_H_INCLUDED
