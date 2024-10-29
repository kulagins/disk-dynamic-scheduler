#ifndef RESTRICT_H
#define RESTRICT_H

#include "graph.hpp"

void bfs(const igraph_t *graph, igraph_integer_t source, igraph_vector_t *rank);

void mixBfsDfs(const igraph_t *graph, igraph_integer_t source, igraph_vector_t *rank, float *alfa);

int sigma_schedule(const igraph_t *graph, igraph_integer_t source, igraph_vector_t *rank ,const igraph_vector_t *capacity,
                igraph_real_t memory_bound);

typedef int edge_selection_function_t(graph_t *, const igraph_t*, igraph_vector_t*, igraph_vector_t*, igraph_vector_t*, igraph_vector_t*, igraph_integer_t, igraph_integer_t, igraph_integer_t*, igraph_integer_t*);

edge_selection_function_t minlevels_2, respectOrder, maxsizes, maxminsize;

#endif
