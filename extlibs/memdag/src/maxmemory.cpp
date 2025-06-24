#include <cstring>
#include "graph.hpp"
#include "restrict.hpp"

/**
 * \file maxmemory.c
 * \brief Compute the maximum topological cut of a graph
 */


/**
 * @private
 *  Compute the maximum topological cut of a DAG stored as a igraph, as described in IPDPS'18
 *
 *
 * @param  graph  the DAG in igraph format
 * @param value pointeur on a double (aka igraph_real_t), where the max cut value will be stored
 * @param cut pointer on a vector that will contain the edges of the cut
 * @param partition pointer on a vector that will contain the S set after the cut
 * @param partition2 pointer on a vector that will contain the T set after the cut
 * @param source id of of the source node of the graph
 * @param target id of of the target node of the graph
 * @param capacity pointer on the vector containing the edge weights
 *
 * NB: Both \p partition and \p partition2 vectors may be NULL, however a vector \p cut
 * must be initialized and provided.
 *
 * @return The function returns the error code of the igraph_st_mincut
 * function used internally, which is zero is everytging went well.
 */


int get_maxcut(const igraph_t *graph, igraph_real_t *value,
         igraph_vector_t *cut, igraph_vector_t *partition,
         igraph_vector_t *partition2,
         igraph_integer_t source, igraph_integer_t target,
         const igraph_vector_t *capacity)
{


  // new vector of capacities: modifcap[i] = - capacity[i]
  igraph_vector_t modifcap;
  igraph_vector_init(&modifcap,0);
  igraph_vector_update(&modifcap,capacity);
  igraph_vector_scale(&modifcap, -1);

  // sum of the capacities, used to initiate the flow
  double initflow = igraph_vector_sum(capacity);



  // go through the edges, add a flow equal to initflow going through each one

  igraph_integer_t edge, a,b;
  igraph_vector_t edges1, edges2;

  igraph_vector_init(&edges1,0);
  igraph_vector_init(&edges2,0);


  for(edge=0; edge < igraph_ecount(graph) ; edge ++)
  {

      // no need to add a flow if this edge already has a large modified capacity
      if ((VECTOR(modifcap)[edge]) > initflow)
        continue;


      igraph_edge(graph, edge, &a, &b); // edge = (a,b)
      // path from s to a
      igraph_get_shortest_path(graph, NULL, &edges1, source, a, IGRAPH_OUT);
      // path from b to t
      igraph_get_shortest_path(graph, NULL, &edges2, b, target, IGRAPH_OUT);
      // add initflow to each edge in the path from s to t through (a,b)
      for(int i=0; i< igraph_vector_size(&edges1); i++)
      {
        int e = VECTOR(edges1)[i];
        VECTOR(modifcap)[ e ] += initflow;
      }
      for(int i=0; i< igraph_vector_size(&edges2); i++)
      {
        int e = VECTOR(edges2)[i];
        VECTOR(modifcap)[e] += initflow;
      }
      VECTOR(modifcap)[ edge ] += initflow;
  }

  // compute the mincut with the new capacities (= maxcut with old ones)
  int errorcode = igraph_st_mincut(graph, value, cut, partition, partition2, source, target, &modifcap);

  // correct the value of the cut with the old capacities
  *value = 0;
  for(int i=0; i< igraph_vector_size(cut); i++)
    {
      int e = VECTOR(*cut)[i];
      *value += VECTOR(*capacity)[e];
    }


  igraph_vector_destroy(&modifcap);
  igraph_vector_destroy(&edges1);
  igraph_vector_destroy(&edges2);

  return errorcode;
}


/**
 * Compute the maximum topological cut of a DAG stored as a igraph, as described in IPDPS'18
 *
 */


double maximum_parallel_memory(graph_t *graph) {
  enforce_single_source_and_target(graph);

  igraph_vector_t edge_weights;
  igraph_t igraph = convert_to_igraph(graph, &edge_weights, NULL, NULL);

  /* We should have vertex->id (in graph) equal to node index (in
   * igraph) This is very probably not true anymore if we add/delete
   * nodes. Since we later only add edges, we SHOULD be able to rely
   * on this.
   */

  // Keeping vertex names and times as follows seems useless
  /* igraph_strvector_t node_names; */
  /* igraph_vector_t vertex_times;  */
  /* igraph_t igraph = convert_to_igraph(graph, &edge_weights, &node_names, &vertex_times); */


  igraph_real_t maxcutvalue;
  igraph_vector_t cut;
  igraph_vector_init(&cut,0);
  /* igraph_vector_init(&S,0); */
  /* igraph_vector_init(&T,0); */

  /* fprintf(stderr,"igraph:\n"); */
  /* igraph_write_graph_edgelist(&igraph, stderr); */
  /* fprintf(stderr,"\n\n source id:%d  target id:%d\n", graph->source->id,  graph->target->id); */

  int error = get_maxcut(&igraph, &maxcutvalue, &cut, NULL, NULL, (igraph_integer_t) graph->source->id, (igraph_integer_t) graph->target->id, &edge_weights);

  fprintf(stderr,"Edges in the maximum topological cut:\n");
  for(int i=0; i< igraph_vector_size(&cut); i++) {
    int e = VECTOR(cut)[i];
    int a,b;
    igraph_edge(&igraph, e, &a, &b);
    fprintf(stderr,"%s -> %s\n", graph->vertices_by_id[a]->name.c_str(), graph->vertices_by_id[b]->name.c_str());
    find_edge(graph->vertices_by_id[a], graph->vertices_by_id[b])->status = IN_CUT;
  }


  if (error) {
    fprintf(stderr,"Error in igraph igraph_st_mincut\n");
  }
  return (double) maxcutvalue;


}





/**
 * @private
 * compute a set of edges to add to a DAG so that its maximum topological cut is not larger than a given bound (from IPDPS'18 paper)
 *
 * Parameters:
 * graph: DAG in igraph format
 * nodework: vector of nodes' processing times
 * capacity: vector of edges weights
 * sigma: initial sequential ordering (needed only for respectOrder)
 * source, target: ids of corresponding vertices
 * A: a function to select which edge to add whenever the max. memory is too large
 */



int restrict_graph(graph_t *graph, igraph_t *igraph, igraph_vector_t *nodework, igraph_vector_t *capacity, igraph_vector_t *sigma,
		   igraph_real_t memory_bound, igraph_integer_t source, igraph_integer_t target, edge_selection_function_t *edge_selection_function) {
  igraph_real_t max_cut;
  igraph_integer_t selected_head_vertex;
  igraph_integer_t selected_tail_vertex;
  igraph_vector_t cut, S, T;
  igraph_vector_init(&cut,0);
  igraph_vector_init(&S,0);
  igraph_vector_init(&T,0);

  get_maxcut(igraph,&max_cut,&cut, &S, &T, source,target,capacity);

  int i=0;

  while(max_cut > memory_bound) {
    i++;

    int success = (*edge_selection_function)(graph, igraph, &cut, sigma, nodework, capacity, source, target, &selected_head_vertex, &selected_tail_vertex);
    if(success == 0) {
	fprintf(stderr,"Restrict: failed after %d iterations\n",i);
	igraph_vector_destroy(&cut);
	igraph_vector_destroy(&S);
	igraph_vector_destroy(&T);
	return 1;
      } else {
      igraph_add_edge(igraph, selected_head_vertex, selected_tail_vertex); // actually adds an edges from 'head' to 'tail'
      igraph_vector_push_back(capacity,0);
      if (graph) {
	edge_t *e = new_edge(graph, graph->vertices_by_id[selected_head_vertex], graph->vertices_by_id[selected_tail_vertex], 0.0, NULL);
	e->status = ADDED;
      }
    }
    get_maxcut(igraph,&max_cut,&cut, &S, &T, source,target,capacity);
  }

  fprintf(stderr,"Restrict: converged after %d iterations\n",i);

  igraph_vector_destroy(&cut);
  igraph_vector_destroy(&S);
  igraph_vector_destroy(&T);
  return 0;
}


/**
 * Computes a set of edges to add to a DAG so that its maximum topological cut is not larger than a given bound (from IPDPS'18 paper)
 *
 * @param graph the original graph, which is modified by the procedure (edges are added)
 * @param memory_bound the given memory bound
 * @param edge_selection_heuristic the heuristic used to select the edges
 *
 * This functions computes the cut, applies the chosen heuristic is
 * the cut is too large, and starts over. The heuristic can be either
 * MIN_LEVEL, RESPECT_ORDER, MAX_SIZE or MAX_MIN_SIZE.
 *
 * The added edges have their status flag set to ADDED.
 *
 * @return The function returns 0 if if succeeds to add edges to meet
 * the memory bound, 1 if it fails after some iterations, and 2 if it
 * is unable to compute an initial ordering for the MIN_LEVEL heuristic.
 *
 */

int add_edges_to_cope_with_limited_memory(graph_t *graph, double memory_bound, edge_selection_heuristic_t edge_selection_heuristic) {
  enforce_single_source_and_target(graph);

  igraph_vector_t edge_weights;
  igraph_vector_t vertex_times;
  igraph_t igraph = convert_to_igraph(graph, &edge_weights, NULL, &vertex_times);

  igraph_vector_t sigma_order;
  igraph_vector_init(&sigma_order, igraph_vcount(&igraph));

  if (edge_selection_heuristic==RESPECT_ORDER) {
    int found = sigma_schedule(&igraph, graph->source->id, &sigma_order, &edge_weights, memory_bound);
    if (!found) {
      fprintf(stderr,"Unable to compute initial sigma schedule for respect_order heuristic\n");
      return 2;
    }
  }

  edge_selection_function_t *edge_selection_function;
  switch (edge_selection_heuristic) {
  case MIN_LEVEL:
    edge_selection_function = &minlevels_2;
    break;
  case RESPECT_ORDER:
    edge_selection_function = &respectOrder;
    break;
  case MAX_SIZE:
    edge_selection_function = &maxsizes;
    break;
  case MAX_MIN_SIZE:
    edge_selection_function = &maxminsize;
    break;
  default:
    fprintf(stderr,"Unkown edge selection heuristic %d\n",edge_selection_heuristic);
    exit(1);
  }

  return restrict_graph(graph, &igraph, &vertex_times, &edge_weights, &sigma_order, (igraph_real_t) memory_bound, graph->source->id, graph->target->id, edge_selection_function);

}
