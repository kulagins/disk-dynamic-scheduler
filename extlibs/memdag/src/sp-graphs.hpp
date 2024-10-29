#ifndef SP_GRAPH_H
#define SP_GRAPH_H

#include "graph.hpp"

/**
 * \file sp-graphs.h
 * \brief types for SP decomposition trees
 */

/**
 * Different types for the node of a SP decomposition tree 
 *
 * Possible cases for a node of a SP decomposition tree: it can be
 * either a leaf node (corresponding to an edge), a series
 * composition, or a parallel composition.
 */

typedef enum { SP_LEAF, SP_SERIES_COMP, SP_PARALLEL_COMP } SP_compo_t;

/**
 * Type for nodes of SP decomposition trees
 *
 * The source and target points to the corresponding vertices of the graph.
 */
typedef struct SP_tree_s {
  int id;
  SP_compo_t type;
  int nb_of_children, children_size;
  vertex_t *source, *target;
  struct SP_tree_s **children;
}  SP_tree_t;


/** Macro to transfor type of a SP decomposition tree node into a letter */
#define COMPOTYPE2STR(t) ((t==SP_LEAF)?"L":((t==SP_SERIES_COMP)?"S":"P"))

///\cond HIDDEN_SYMBOLS
SP_tree_t *SP_tree_new_node(SP_compo_t type);
void SP_tree_append_child(SP_tree_t *tree, SP_tree_t *child);
void SP_tree_prepend_child(SP_tree_t *tree, SP_tree_t *child);
void SP_tree_remove_child(SP_tree_t *tree, int child_index);
void SP_tree_free(SP_tree_t *tree);
void SP_tree_give_unique_ids(SP_tree_t *tree);

graph_t *SP_tree_to_graph(SP_tree_t *tree);

SP_tree_t *build_SP_decomposition_tree(graph_t *graph);

vertex_t **compute_optimal_SP_traversal(graph_t  *sp_graph, SP_tree_t *tree);

graph_t *graph_sp_ization(graph_t *graph);

///\endcond
#endif
