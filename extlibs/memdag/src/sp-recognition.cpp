#include <stdlib.h>
#include <stdio.h>
#include "fifo.hpp"
#include "graph.hpp"
#include "sp-graph.hpp"

//#define DEBUG_SP_RECO


/** \file sp-recognition.c
 * \brief Recognize Series-Parallel graphs
 */
///@private
SP_tree_t *SP_tree_new_node(SP_compo_t type) {
  SP_tree_t *node = (SP_tree_t*) calloc(1,sizeof(SP_tree_t));
  node->type=type;
  node->nb_of_children = 0;
  if (type == SP_LEAF) {
    node->children_size = 0;
  } else {
    node->children_size = 4;
    node->children = (SP_tree_t**) calloc(node->children_size,sizeof(SP_tree_t*));
  }
  //fprintf(stderr,"New tree node %p of type %s (n=%d)\n",(void*)node, COMPOTYPE2STR(node->type), node->nb_of_children);
  return node;
}

///@private
void SP_tree_append_child(SP_tree_t *tree, SP_tree_t *child) {
  if (tree->nb_of_children == tree->children_size) {
    //    fprintf(stderr,"SP tree needs to reallocate children\n");
    tree->nb_of_children = tree->nb_of_children * 2;
    tree->children = (SP_tree_t **) realloc(tree->children, tree->nb_of_children * sizeof (SP_tree_t*));
  }
  tree->children[tree->nb_of_children] = child;
  tree->nb_of_children++;
  /* fprintf(stderr,"Tree %p (type %s) has now %d children (the new one is %p, type %s, n=%d)\n", */
  /* 	  (void*) tree, COMPOTYPE2STR(tree->type),tree->nb_of_children, */
  /* 	  (void*) child, COMPOTYPE2STR(child->type),child->nb_of_children); */
}


// NB: commented out SP_tree_prepend_child and SP_tree_remove_child which are not used

/* void SP_tree_prepend_child(SP_tree_t *tree, SP_tree_t *child) { */
/*   if (tree->nb_of_children == tree->children_size) { */
/*     tree->nb_of_children = tree->nb_of_children * 2; */
/*     tree->children = (SP_tree_t **) realloc(tree->children, tree->nb_of_children * sizeof (SP_tree_t*)); */
/*   } */
/*   for(int i=0; i<tree->nb_of_children; i++) { */
/*     tree->children[i+1] = tree->children[i]; */
/*   } */
/*   tree->nb_of_children++; */
/*   tree->children[0] = child; */
/*   //fprintf(stderr,"Tree %p (type %s) has now %d children\n",(void*) tree, COMPOTYPE2STR(tree->type),tree->nb_of_children); */
/* } */

/* void SP_tree_remove_child(SP_tree_t *tree, int child_index) { */
/*   for(int i=child_index+1; i<tree->nb_of_children; i++) { */
/*     tree->children[i-1] = tree->children[i]; */
/*   } */
/*   tree->nb_of_children--; */
/*   tree->children[tree->nb_of_children] = NULL; */
/* } */

///@private
void SP_tree_free(SP_tree_t *tree) {
  for(int i=0; i<tree->nb_of_children; i++) {
    SP_tree_free(tree->children[i]);
  }
  free(tree);
}


///@private
static vertex_t *SP_tree_add_nodes_to_graph(graph_t *graph, SP_tree_t *tree) {
  char nodename[255];
  //snprintf(nodename,255,"%d %s (%s -> %s)",tree->id, COMPOTYPE2STR(tree->type), tree->source_name, tree->target_name);
  snprintf(nodename,255,"%s (%s -> %s)", COMPOTYPE2STR(tree->type), tree->source->name.c_str(), tree->target->name.c_str());
  //  fprintf(stderr,"SP TREE seen node %p: \"%s\"\n", (void*)tree, nodename);
  vertex_t *root = new_vertex(graph, nodename, 0.0, (void*) tree);
  tree->id = root->id;
  for(int i=0; i<tree->nb_of_children; i++) {
    vertex_t *child = SP_tree_add_nodes_to_graph(graph, tree->children[i]);
    new_edge(graph, root, child, (double)i, NULL);
  }
  return root;
}

///@private
graph_t *SP_tree_to_graph(SP_tree_t *tree) {
  graph_t *graph = new_graph();
  SP_tree_add_nodes_to_graph(graph, tree);
  return graph;
}

#ifdef DEBUG_SP_RECO
///@private
static void SP_tree_print_debug(SP_tree_t *tree, FILE *out, int level) {
  char *s = (char*)calloc(level*3+1, sizeof(char));
  for(int i=0; i<level; i++) {
    s[3*i] = '|';
    s[3*i+1] = ' ';
    s[3*i+2] = ' ';
    s[3*i+3] = 0;
  }
  fprintf(out,"%d ",level);
  fprintf(out,"%s%s (%s -> %s)", s,
	  COMPOTYPE2STR(tree->type),
	  tree->source->name, tree->target->name);
  if (tree->nb_of_children>0)
    fprintf(out,", %d children:", tree->nb_of_children);
  fprintf(out,"\t\t\t\t\t\t\t%p\n", (void*)tree);
  free(s);
  for(int i=0; i<tree->nb_of_children; i++) {
    SP_tree_print_debug(tree->children[i], out, level + 1 );
  }
}
#endif

///@private
static void SP_minimize_tree(SP_tree_t *tree) {
  int new_nb_of_children = tree->nb_of_children;
  SP_tree_t **new_children;
  for(int i=0; i<tree->nb_of_children; i++) {
    SP_minimize_tree(tree->children[i]);
    if (tree->children[i]->type == tree->type) {
      new_nb_of_children += tree->children[i]->nb_of_children - 1;
    }
  }

  if (new_nb_of_children > tree->nb_of_children) {
    int new_children_index = 0;
    new_children = (SP_tree_t **) calloc(new_nb_of_children,sizeof(SP_tree_t*));
    for(int i=0; i<tree->nb_of_children; i++) {
      SP_tree_t *child=tree->children[i];
      if (child->type == tree->type) {
	for(int j=0; j<child->nb_of_children; j++) {
	  new_children[new_children_index] = child->children[j];
	  new_children_index++;
	}
	free(child->children); free(child);
      } else {
	new_children[new_children_index] = child;
	new_children_index++;
      }
    }
    SP_tree_t **old_children = tree->children;
    tree->children = new_children;
    tree->nb_of_children = new_nb_of_children;
    free(old_children);
  }
}

/**
 *  Recognize a SP graph and builds its decomposition tree
 * @param original_graph
 *
 * If \p original_graph is a SP graph, then its decomposition tree is
 * built (using the algorithm from J. Valdes, R. Tarjan, E. Lawlers
 * The Recognition of Series Parallel Digraphs, SIAM J. Comput. 11,
 * 1982, 298-313) and returned. Otherwise, NULL is returned.
 */

SP_tree_t *build_SP_decomposition_tree(graph_t *original_graph) {
  /*
   * Make sure the graph has a single source and a single target
   */

  if (!(original_graph->source)) {
    enforce_single_source_and_target(original_graph);
  }

  /*
   * We work on a copy of the graph, since we need to modify the graph. We rely on the fact that ids are conserved by the copy.
   */

  graph_t *graph = copy_graph(original_graph, 0);

  /* out=fopen("graph_copied.dot","w"); */
  /* print_graph_to_dot_file(graph,out); */
  /* fclose(out); */

  /*
   * Associate a partial tree to each edge of the graph, initialized with a leaf tree representing the edge
   */
  ACQUIRE(graph->generic_edge_pointer_lock);
  ACQUIRE(graph->generic_vertex_int_lock);


  for(edge_t *e=graph->first_edge; e; e=e->next) {
    SP_tree_t *tree_node = SP_tree_new_node(SP_LEAF);
    tree_node->source = original_graph->vertices_by_id[e->tail->id];
    tree_node->target = original_graph->vertices_by_id[e->head->id];
    e->generic_pointer = (void*) tree_node;
  }


  /*
   * Create and fill the list of unsatisfied nodes. We also associate
   * a flag to each node to know if it is already in the list, for
   * faster insertion.
   */
  fifo_t *unsatisfied_nodes = fifo_new();
  for(vertex_t *v=graph->first_vertex; v; v=v->next) {
    if ((not v->in_edges.empty()) && (not v->out_edges.empty())) {
      v->generic_int = 1; // used as unsatisfied flag
      fifo_write(unsatisfied_nodes, (void*)v);
    }
  }

  // TODO: deal with multiple edges

  /*
   * Process unsatisfied nodes
   */

  while(!fifo_is_empty(unsatisfied_nodes)) {
    vertex_t *v = (vertex_t*) fifo_read(unsatisfied_nodes);
    v->generic_int = 0;
    // there are no parallel edges -> try only series reduction
    if ((v->in_edges.size() == 1) && (v->out_edges.size() == 1)) {
      // remove node, incoming and outgoing edges and add direct edge if it does not exist yet
      edge_t *in_edge = v->in_edges[0];
      vertex_t *in_vertex = in_edge->tail;
      edge_t *out_edge = v->out_edges[0];
      vertex_t *out_vertex = out_edge->head;
      // Create the partial tree corresponding to the series composition
      SP_tree_t *series_compo_tree = SP_tree_new_node(SP_SERIES_COMP);
      SP_tree_append_child(series_compo_tree, (SP_tree_t*) (in_edge->generic_pointer));
      SP_tree_append_child(series_compo_tree, (SP_tree_t*) (out_edge->generic_pointer));
      series_compo_tree->source = ((SP_tree_t*) (in_edge->generic_pointer))->source;
      series_compo_tree->target = ((SP_tree_t*) (out_edge->generic_pointer))->target;

#ifdef DEBUG_SP_RECO
      fprintf(stderr,"\nRemoving node %s...\nSeries_compo_tree:\n",v->name);
      SP_tree_print_debug(series_compo_tree, stderr,0);
#endif

      remove_edge(graph, in_edge);
      remove_edge(graph, out_edge);
      remove_vertex(graph, v);

      // NB: we do not add an edge which is already present, to avoid the need for parallel reductions
      edge_t *existing_edge = find_edge(in_vertex, out_vertex);
      if (!existing_edge) {
	edge_t *e=new_edge(graph, in_vertex, out_vertex, 0, NULL);
	e->generic_pointer =  series_compo_tree;
#ifdef DEBUG_SP_RECO
	fprintf(stderr,"Connecting %s -> %s with new edge\n",in_vertex->name, out_vertex->name);
#endif
      } else {
	// Replace the partial tree t of the existing edge by a parallel composition of t and the new tree
	/* TODO V2: if the existing edge is already a parallel
	   composition, we should add a branch to it, rather than a
	   new parallel composition on top of it. Then, we would not
	   need the minimization of the tree anymore. */
	SP_tree_t *parallel_compo_tree = SP_tree_new_node(SP_PARALLEL_COMP);
	SP_tree_append_child(parallel_compo_tree, (SP_tree_t*) (existing_edge->generic_pointer));
	SP_tree_append_child(parallel_compo_tree, series_compo_tree);
	parallel_compo_tree->source = series_compo_tree->source;
	parallel_compo_tree->target = series_compo_tree->target;
	existing_edge->generic_pointer = (void*) parallel_compo_tree;
#ifdef DEBUG_SP_RECO
	fprintf(stderr,"\nEdge %s -> %s already exists - updated parallel_compo_tree:\n",in_vertex->name, out_vertex->name);
	SP_tree_print_debug(parallel_compo_tree, stderr,0);
#endif
      }
      if ((in_vertex->generic_int==0) && (in_vertex != graph->source)) {
	fifo_write(unsatisfied_nodes, (void*) in_vertex);
	in_vertex->generic_int = 1;
      }
      if ((out_vertex->generic_int==0) && (out_vertex != graph->target)) {
	fifo_write(unsatisfied_nodes, (void*) out_vertex);
	out_vertex->generic_int = 1;
      }
    }
  }
  fifo_free(unsatisfied_nodes);

  //fprintf(stderr,"We are done with the reduction.\n");

  // If it was a SP-graph, it should now be a single edge source->sink
  SP_tree_t *result = NULL;
  if ((graph->number_of_vertices==2) && (graph->number_of_edges==1)) {
    edge_t *single_edge = graph->first_edge;
    SP_tree_t *tree = (SP_tree_t*) single_edge->generic_pointer;
    SP_minimize_tree(tree);
    result = tree;
  }

  RELEASE(graph->generic_edge_pointer_lock);
  RELEASE(graph->generic_vertex_int_lock);
  free_graph(graph); // Remove copy
#ifdef DEBUG_SP_RECO
  fprintf(stderr,"\nFinal composition tree:\n");
  SP_tree_print_debug(result, stderr,0);
#endif


  return result;
}
