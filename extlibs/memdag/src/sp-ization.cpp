#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "fifo.hpp"
#include "graph.hpp"
#include "sp-graph.hpp"

//#define DEBUG_SP_IZATION

/**
 * \file sp-ization.c
 * \brief Tools to transform a graph into a Series-Parallel graph
 *
 * Transform a graph into a SP graph such that any schedule of the SP
 * graph can be applied to the original graph and will result in the
 * same memory peak. The memory peak of the SP-graph is an upper bound
 * on the memory peak of the original graph.
 *
 * Note: the provenance of the data is not taken care of. Later, we
 * could produce a description, stating how each data of the original
 * graph is considered in the SP graph
 */

///@private
typedef struct parallel_tree_node_s {
  struct parallel_tree_node_s *parent;
  int number_of_children;
  struct parallel_tree_node_s **children;
  vertex_t *source; // either where the fork is or the tail of the edge
  vertex_t *target; // either the head of the edge or NULL
  int label;
} parallel_tree_node_t;


///@private
parallel_tree_node_t *new_leaf(parallel_tree_node_t *parent, vertex_t *source, vertex_t *target) {
  parallel_tree_node_t *node = (parallel_tree_node_t*) calloc(1,sizeof(parallel_tree_node_t));
  node->parent = parent;
  node->source = source;
  node->target = target;
  return node;
}

///@private
parallel_tree_node_t *find_lca(parallel_tree_node_t *n1, parallel_tree_node_t *n2) { // uses node labels
  if (n1==NULL)
    return n2;
  if (n2==NULL)
    return n1;

  parallel_tree_node_t *lca = NULL;
  for(parallel_tree_node_t *n = n1; n; n=n->parent) {
    n->label = 1;
  }
  for(parallel_tree_node_t *n = n2; n; n=n->parent) {
    if (n->label) {
      lca = n;
      break;
    }
  }
  for(parallel_tree_node_t *n = n1; n; n=n->parent) {
    n->label = 0;
  }
  return lca;
}

///@private
void put_subtree_edges_in_fifo(parallel_tree_node_t *root, fifo_t *fifo) {
  if (root->number_of_children == 0) {
#ifdef DEBUG_SP_IZATION
    fprintf(stderr,"    Edge to be closed: %s->%s (%p)\n", root->source->name, root->target->name, (void*) root);
#endif
    fifo_write(fifo, root);
  } else {
    for(int i=0; i<root->number_of_children; i++)
      put_subtree_edges_in_fifo(root->children[i], fifo);
  }
}


/* int number_of_leaves_in_subtree(parallel_tree_node_t *root) { */
/*   if (root->number_of_children == 0) */
/*     return 1; */
/*   int n = 0; */
/*   for(int i=0; i<root->number_of_children; i++) */
/*     n += number_of_leaves_in_subtree(root->children[i]); */
/*   return n; */
/* } */

/* parallel_tree_node_t *next_leaf_of_subtree(parallel_tree_node_t *root, parallel_tree_node_t *current_leaf) { // uses node labels */
/*   parallel_tree_node_t *current_node = NULL; */
/*   // Phase 1: going up */
/*   if (current_leaf != NULL) { //label = next child index to visit, incremented when starting the visit of one child */
/*     current_node = current_leaf; */
/*     while (current_node->label == current_node->number_of_children) { */
/*       current_node->label = 0; // re-initialize label for next traversal */
/*       if (current_node == root) { //done! */
/* 	return NULL; */
/*       } */
/*       current_node = current_node->parent; */
/*     } */
/*   } else { */
/*     current_node = root; */
/*   } */
/*   // Phase 2: going down */
/*   while (current_node->number_of_children > 0) { */
/*     parallel_tree_node_t *next = current_node->children[current_node->label]; */
/*     current_node->label++; */
/*     current_node = next; */
/*   } */
/*   return current_node; */
/* } */

///@private
void free_composition_tree(parallel_tree_node_t *tree) {
  for(int i=0; i<tree->number_of_children; i++) {
    free_composition_tree(tree->children[i]);
  }
  if (tree->number_of_children>0)
    free(tree->children);
  free(tree);
}

///@private
parallel_tree_node_t *replace_subtree(parallel_tree_node_t *tree, parallel_tree_node_t *old_subtree, parallel_tree_node_t *new_subtree) {
  if (old_subtree == tree) {
    free(old_subtree);
    new_subtree->parent = NULL;
    return new_subtree;
  }
  assert(old_subtree->parent);
  int old_root_index_in_parent = 0;
  while (old_subtree->parent->children[old_root_index_in_parent] != old_subtree) {
    old_root_index_in_parent++;
    assert(old_root_index_in_parent < old_subtree->parent->number_of_children);
  }
  old_subtree->parent->children[old_root_index_in_parent] = new_subtree;
  new_subtree->parent = old_subtree->parent;
  free_composition_tree(old_subtree);
  return tree;
}

///@private
void remove_subtrees(fifo_t *subtree_to_remove_fifo) {
  while (!fifo_is_empty(subtree_to_remove_fifo)) {
    parallel_tree_node_t *node_to_remove =
      static_cast<parallel_tree_node_t*>(fifo_read(subtree_to_remove_fifo));
    parallel_tree_node_t *parent =
      static_cast<parallel_tree_node_t*>(node_to_remove->parent);
    int child_index = 0;
    while(parent->children[child_index] != node_to_remove) {
      child_index++;
    }
    free_composition_tree(node_to_remove);

    for(int i=child_index+1; i<parent->number_of_children; i++) {
      parent->children[i-1] = parent->children[i];
    }
    parent->number_of_children--;
    parent->children[parent->number_of_children] = NULL;
  }
}

/**
 * Transform a graph to a SP-graph
 *
 * The original graph is not modified. A copy is made, on which
 * synchronisation vertices and edges are added, and some edges
 * deleted (replaced by paths going through synchronisation
 * nodes). The algorithm is a variant of the algorithm from
 * González-Escribano, A., Van Gemund, A. J., & Cardeñoso-Payo,
 * V. (2002, June). Mapping Unstructured Applications into Nested
 * Parallelism. In International Conference on High Performance
 * Computing for Computational Science (VECPAR'02),
 * pp. 407-420. Springer, Berlin, Heidelberg./

 *
 * @return the SP-ized graph
 */
graph_t *graph_sp_ization(graph_t *original_graph) { // Uses edges' generic_pointer
  graph_t *graph = copy_graph(original_graph, 0);

  enforce_single_source_and_target(graph);
  merge_multiple_edges(graph);
  remove_transitivity_edges_weight_conservative(graph);
  compute_bottom_and_top_levels(graph);

  parallel_tree_node_t *parallel_tree = new_leaf(NULL, NULL, graph->source); // mimics fake edge from NULL to source
  ACQUIRE(graph->generic_edge_pointer_lock);
  for(edge_t *e = graph->first_edge; e; e=e->next) {
    e->generic_pointer = NULL;
  }


  int sync_vertex_counter = 0;

  // 0. Pre-compute a list of sorted vertices. This is needed as we
  // will transform the graph while traversing it, and we do not want
  // to disturb the traversal.

  fifo_t *sorted_vertices = fifo_new();
  for(vertex_t *current_vertex = graph->source; current_vertex; current_vertex = next_vertex_in_sorted_topological_order(graph, current_vertex, sort_by_increasing_avg_level)) {
    fifo_write(sorted_vertices, current_vertex);
  }

  while (!fifo_is_empty(sorted_vertices)) {
    vertex_t *current_vertex = static_cast<vertex_t*>(fifo_read(sorted_vertices));

    // When merging several branch, check for SP problems.  In all
    // cases, find the branch where the current node is (after merging
    // all incoming branches)
    parallel_tree_node_t *tree_branch_of_current_vertex = NULL;
    if (current_vertex->in_degree > 1) {

      // 1. Find the LCA of all branches closed by the fork
      parallel_tree_node_t *lca = NULL;
      for(int i=0; i<current_vertex->in_degree; i++) {
	assert(current_vertex->in_edges[i]->generic_pointer);
	lca = find_lca(lca, static_cast<parallel_tree_node_t*>(current_vertex->in_edges[i]->generic_pointer));
      }
#ifdef DEBUG_SP_IZATION
      fprintf(stderr, "Merge at vertex %s, LCA is vertex %s, with children:\n",current_vertex->name, lca->source->name);
      for(int i=0; i<lca->number_of_children; i++)
	fprintf(stderr, "    * child at %s with %d children\n", lca->children[i]->source->name, lca->children[i]->number_of_children);
#endif

      // 1.bis Select the branches of the LCA which have to be closed
      // during this merge, as well as all their leaves.
      fifo_t *leaves_to_be_closed = fifo_new();
      fifo_t *successors_of_lca_to_remove = fifo_new();
      for(int i=0; i<current_vertex->in_degree; i++) {
	parallel_tree_node_t *successor_of_lca = static_cast<parallel_tree_node_t*>(current_vertex->in_edges[i]->generic_pointer);
	while (successor_of_lca->parent != lca) {
	  successor_of_lca = successor_of_lca->parent;
	}
	if (!fifo_is_in(successors_of_lca_to_remove, successor_of_lca)) {
	  fifo_write(successors_of_lca_to_remove, successor_of_lca);
#ifdef DEBUG_SP_IZATION
	  fprintf(stderr,"  Sucessor %s (%p)\n", successor_of_lca->source->name, (void*) successor_of_lca);
#endif
	  put_subtree_edges_in_fifo(successor_of_lca, leaves_to_be_closed);
	}
      }


      // 2. Check if we have a SP problem
      int number_of_leaves_to_synchronise = fifo_size(leaves_to_be_closed);
      parallel_tree_node_t *node_result_of_the_merge = NULL;
      if (number_of_leaves_to_synchronise > current_vertex->in_degree) {
#ifdef DEBUG_SP_IZATION
	fprintf(stderr,"  SP problem at node %s: %d inputs vs. %d leaves to be closed from root %s\n",
		current_vertex->name, current_vertex->in_degree, number_of_leaves_to_synchronise, lca->source->name);
#endif

	// 3. Create a new synchronisation vertex s in the graph
	char sync_name[255];
	snprintf(sync_name, 254, "SYNC_%d_%s", sync_vertex_counter++, lca->source->name.c_str());
	vertex_t *sync_vertex = new_vertex(graph, sync_name, 0, NULL);

	// 4. Create the corresponding node in the tree
	// NB: We first estimate its number of children: this is an
	// upper bound on the actual number of children. First, the
	// inputs of current_vertex will be replaced by the
	// current_vertex itself. Second, some of the edges may share
	// the same target (different from the current vertex), so we
	// may end up with less edges. This number will be corrected
	// afterwards.

	parallel_tree_node_t *sync_node = new_leaf(NULL, sync_vertex, NULL); // NB: the parent will be set later (to lca or lca->parent)
	sync_node->number_of_children = number_of_leaves_to_synchronise - current_vertex->in_degree + 1; // Upper bound
#ifdef DEBUG_SP_IZATION
	fprintf(stderr,"    Added synchronisation node %s, upper bound on its number of children: %d\n",sync_vertex->name,sync_node->number_of_children);
#endif
	sync_node->children = (parallel_tree_node_t**) calloc(sync_node->number_of_children, sizeof(parallel_tree_node_t*));


	// 5. For each leaf corresponsing to an edge i->j in the
	// subtree rooted at the LCA, make it go through the
	// synchronisation node: i->s->j. We never create multiple
	// edges, so some edges' data may be lost, and weights are
	// accumulated when needed.

	int next_sync_node_child_index = 0;
	while (!fifo_is_empty(leaves_to_be_closed)) {
	  parallel_tree_node_t *leaf = static_cast<parallel_tree_node_t*>(fifo_read(leaves_to_be_closed));
	  edge_t *edge_to_be_removed = find_edge(leaf->source, leaf->target);
	  double edge_weight = edge_to_be_removed->weight;

#ifdef DEBUG_SP_IZATION
	  fprintf(stderr,"    Replacing edge %s->%s by path %s->%s->%s\n", leaf->source->name, leaf->target->name, leaf->source->name, sync_vertex->name, leaf->target->name);
#endif
	  remove_edge(graph, edge_to_be_removed);

	  edge_t *first_edge = find_edge(leaf->source, sync_vertex);
	  if (first_edge) {
	    first_edge->weight += edge_weight;
	  } else {
	    new_edge(graph, leaf->source, sync_vertex, edge_weight, NULL);
	  }

	  edge_t *second_edge = find_edge(sync_vertex, leaf->target);
	  if (second_edge) {
	    second_edge->weight += edge_weight;
	  } else {
	    second_edge = new_edge(graph, sync_vertex, leaf->target, edge_weight, NULL);
	    // Create a leaf of s in the tree corresponding to the newly created edge s->j
	    parallel_tree_node_t *s_leaf = new_leaf(sync_node, sync_vertex, leaf->target);
	    sync_node->children[next_sync_node_child_index] = s_leaf;
	    next_sync_node_child_index++;
	    second_edge->generic_pointer = s_leaf;
	    if (leaf->target == current_vertex)
	      tree_branch_of_current_vertex = s_leaf;
	  }
	}
	// Finally update the number of children of sync_node
	sync_node->number_of_children = next_sync_node_child_index;

	node_result_of_the_merge = sync_node;
      } else {
	// A regular merge, without SP problem, results in a single single leaf, representing a
	// fictitious edge from LCA to the current vertex.
	node_result_of_the_merge = new_leaf(NULL, lca->source, current_vertex); // NB: the parent will be set later (to lca or lca->parent)
	tree_branch_of_current_vertex = node_result_of_the_merge;
#ifdef DEBUG_SP_IZATION
	fprintf(stderr,"  Standard merge at vertex %s, removed subtree from fork at node %s\n", current_vertex->name, lca->source->name);
#endif
      }
      // 6. If all branches of the LCA were close, remove the whole
      // subtree rooted at LCA, replace it by the one just
      // created. Otherwise, remove only these branches and add the
      // node resulting from the merge as a successor of LCA.
      if (lca->number_of_children == fifo_size(successors_of_lca_to_remove)) {
#ifdef DEBUG_SP_IZATION
	fprintf(stderr,"  Result of the merge: replace LCA at %s by result node\n",lca->source->name);
#endif
	parallel_tree = replace_subtree(parallel_tree, lca, node_result_of_the_merge);
      } else {
#ifdef DEBUG_SP_IZATION
	fprintf(stderr,"  Result of the merge: remove only %d branches of LCA at %s and add one for the result of the merge\n",
		fifo_size(successors_of_lca_to_remove), lca->source->name);
#endif
	parallel_tree = replace_subtree(parallel_tree, static_cast<parallel_tree_node_t*>(fifo_read(successors_of_lca_to_remove)), node_result_of_the_merge);
	remove_subtrees(successors_of_lca_to_remove);
      }
      fifo_free(successors_of_lca_to_remove);
      fifo_free(leaves_to_be_closed);

    } else if (current_vertex->in_degree == 1) {
      tree_branch_of_current_vertex = static_cast<parallel_tree_node_t*>(current_vertex->in_edges[0]->generic_pointer);
    } else { // current_vertex->in_degree == 0, special case for the source node
      tree_branch_of_current_vertex = parallel_tree;
    }

    parallel_tree_node_t *current_node = tree_branch_of_current_vertex;
#ifdef DEBUG_SP_IZATION
    //    fprintf(stderr,"At node %s between merge and fork, current_node=%p\n", current_vertex->name, (void*)current_node);
#endif

    // In case of a fork, create new children to the current node of
    // the tree to account for the new edges.
    if (current_vertex->out_degree > 1) {
      // Transform current_node from leaf to inner node
      current_node->source = current_vertex;
      current_node->target = NULL;
      current_node->number_of_children = current_vertex->out_degree;
      current_node->children = (parallel_tree_node_t**) calloc(current_node->number_of_children, sizeof(parallel_tree_node_t*));
#ifdef DEBUG_SP_IZATION
      fprintf(stderr,"While processing vertex %s, transformed leaf into inner node with %d children:\n", current_vertex->name, current_vertex->out_degree);
#endif
      for(int i=0; i<current_vertex->out_degree; i++) {
	parallel_tree_node_t *leaf = new_leaf(current_node, current_vertex, current_vertex->out_edges[i]->head);
	current_node->children[i] = leaf;
	current_vertex->out_edges[i]->generic_pointer = leaf;
#ifdef DEBUG_SP_IZATION
	fprintf(stderr,"  - leaf %s->%s\n", current_vertex->name, current_vertex->out_edges[i]->head->name);
#endif
      }
    }
    if (current_vertex->out_degree == 1) { // Update the (source,target) pair of the leaf node
#ifdef DEBUG_SP_IZATION
      fprintf(stderr,"Updating leaf from %s->%s to %s->%s\n",
	      current_node->source->name, current_node->target->name,
	      current_vertex->name, current_vertex->out_edges[0]->head->name);
#endif
      current_node->source = current_vertex;
      current_node->target = current_vertex->out_edges[0]->head;
      current_vertex->out_edges[0]->generic_pointer = current_node;
    }
  }
  fifo_free(sorted_vertices);

  for(edge_t *e = graph->first_edge; e; e=e->next) {
    e->generic_pointer = NULL;
  }
  RELEASE(graph->generic_edge_pointer_lock);

  return graph;
}

/*
 * graph_sp_ization_lower_bound
 *
 * Same, but now produces a lower bound on the memory needed for the
 * graph: any schedule of the original graph can be applied to the
 * SP-graph and result in the same memory peak. Will be useful to
 * compute under-estimates in the A* search for optimal schedules.
 */


/* graph_t *graph_sp_ization_lower_bound(graph_t *original_graph) { // maybe a variant of the previous one (option) ? */

/*   return NULL; */
/* } */
