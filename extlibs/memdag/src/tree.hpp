#ifndef TREES_H
#define TREES_H

#include "graph.hpp"

/**
 * \file trees.h
 * \brief tree definition, management and algorithms
 */


/**
 * Tree node type
 *
 * Vertices are organised in a double-linked list, hence the \p prev
 * and \p next pointers. Arrays \p in_edges and \p out_edges
 * contains pointers to the input and output edges. Their size is
 * given by \p in_size and \p out_size, however they contain only
 * \p in_degree and \p out_degree edges.
 *
 * \p data is a pointer reserved for the user's usage.
 */

 typedef struct s_node_t {
   /* basic node information */
   int id;
   char *name;
   double out_size;      
   double time;
   int nb_of_children;      // number of childre of this node
   int max_nb_of_children;  // allocated size of "children", for initialization only
   
   /* computed values to speed up algorithms */
   double total_input_size; 
   double critical_path;    // sum of the processing times of the nodes on the path from this node to the root
   double max_subtree_critical_path;  // maximum of the sum of the processing times of all nodes on any path going from this node to a leaf
   double subtree_time; // sum of the processing times in the subtree rooted at current node
   int subtree_size;        // number of nodes in the subtree rooted at this node

   /* pointers to children and parent */
   struct s_node_t **children; // array of pointers to the children
   struct s_node_t *parent;  // pointer to the father

} node_t;

typedef node_t * tree_t; // Warning: tree[i].id = i must be verified !

#define newn(type,nb) ((type*)calloc(nb,sizeof(type)))
typedef enum {OUT_OF_CORE=1, IN_CORE=2} incore_switch_t;
typedef enum {LIU_MAX_MODEL=1, TOPC_SUM_MODEL=2} model_switch_t;

tree_t read_tree_as_node_list(const char *filename);
tree_t copy_tree(tree_t tree);

void print_tree_as_node_list(FILE *out, tree_t tree);
void print_dot_tree(FILE *out, tree_t tree);
void free_tree(tree_t tree);


int check_if_tree(graph_t *graph, int add_common_root_if_forest);
tree_t graph_to_tree(graph_t *graph);
graph_t *tree_to_graph(tree_t tree);

double Liu_optimal_sequential_memory(tree_t tree, int **schedule); // returns the memory. if schedule!=null, it is allocated and nodes id are copied in the correct order
double best_postorder_sequential_memory(tree_t tree, int **schedule); // returns the memory. if schedule!=null, it is allocated and nodes id are copied in the correct order


#endif // TREES_H
