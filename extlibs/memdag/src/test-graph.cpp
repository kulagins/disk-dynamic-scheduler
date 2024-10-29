#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <graphviz/cgraph.h>
#include "fifo.hpp"
#include "graph.hpp"
#include "sp-graph.hpp"
#include "heap.hpp"

void simple_graph_test(void) {
  graph_t *g=new_graph();
  vertex_t *v1 = new_vertex(g, "V_one", 1.0, NULL);
  vertex_t *v2 = new_vertex(g, "V_two", 2.0,  NULL);
  vertex_t *v3 = new_vertex(g, "V_three", 3.0,  NULL);
  vertex_t *v4 = new_vertex(g, "V_four", 4.0,  NULL);
  vertex_t *v5 = new_vertex(g, "V_five", 5.0,  NULL);
  edge_t *e1 = new_edge(g, v1, v2, 12, NULL);
  edge_t *e2 = new_edge(g, v1, v3, 8, NULL);
  edge_t *e3 = new_edge(g, v2, v3, 4, NULL);
  edge_t *e4 = new_edge(g, v2, v3, 6, NULL);
  edge_t *e5 = new_edge(g, v2, v4, 3, NULL);
  edge_t *e6 = new_edge(g, v3, v5, 1, NULL);
  edge_t *e7 = new_edge(g, v4, v5, 1, NULL);


  
  
  enforce_single_source_and_target(g);
  fprintf(stdout,"Source:%s  Target:%s\n",g->source->name, g->target->name);

  assert(check_if_path_exists(g->source, g->target));
  assert(check_if_path_exists(g->target, g->source)==0);
	 
  
  fprintf(stdout,"Original graph:\n");
  print_graph_to_dot_file(g,stdout);

  remove_transitivity_edges_weight_conservative(g);
  fprintf(stdout,"Graph after removing transitivity edges:\n");
  print_graph_to_dot_file(g,stdout);

  merge_multiple_edges(g);
  fprintf(stdout,"Graph after merging multiple edges:\n");
  print_graph_to_dot_file(g,stdout);


  fprintf(stdout,"Nodes in topological order:");
  for(vertex_t *u=g->source; u; u=next_vertex_in_topological_order(g,u))
    fprintf(stdout,"%s ",u->name);
  fprintf(stdout,"\n");

  fprintf(stdout,"Nodes in anti-topological order:");
  for(vertex_t *u=g->target; u; u=next_vertex_in_anti_topological_order(g,u))
    fprintf(stdout,"%s ",u->name);
  fprintf(stdout,"\n");

  compute_bottom_and_top_levels(g);
  for(vertex_t *u=first_vertex(g); is_last_vertex(u); u=next_vertex(u))
    fprintf(stdout,"Node %s  \thas top-level:%.0f   \tand bottom-level:%.0f\n",u->name, u->top_level, u->bottom_level);

  fprintf(stdout,"Nodes in topological order, sorted by increasing top_level:                ");
  for(vertex_t *u=g->source; u; u=next_vertex_in_sorted_topological_order(g,u,&sort_by_increasing_top_level))
    fprintf(stdout,"%s  ",u->name);
  fprintf(stdout,"\n");

  fprintf(stdout,"Nodes in topological order, sorted by decreasing bottom_level:             ");
  for(vertex_t *u=g->source; u; u=next_vertex_in_sorted_topological_order(g,u,&sort_by_decreasing_bottom_level))
    fprintf(stdout,"%s  ",u->name);
  fprintf(stdout,"\n");
  
  fprintf(stdout,"Nodes in topological order, sorted by increasing top_level - bottom_level: ");
  for(vertex_t *u=g->source; u; u=next_vertex_in_sorted_topological_order(g,u,&sort_by_increasing_avg_level))
    fprintf(stdout,"%s  ",u->name);
  fprintf(stdout,"\n");

  
  edge_t *e8 = new_edge(g, v3, v4, 1, NULL);
  vertex_t *v6 = new_vertex(g, "V_six", 6.0,  NULL);
  edge_t *e9 = new_edge(g, v3, v6, 1, NULL);
  edge_t *e10 = new_edge(g, v6, v5, 1, NULL);
  vertex_t *v7 = new_vertex(g, "V_seven", 6.0,  NULL);
  edge_t *e11 = new_edge(g, v2, v7, 1, NULL);
  edge_t *e12 = new_edge(g, v7, v4, 1, NULL);

  graph_t *gg=copy_graph(g,0);
  delete_transitivity_edges(gg);
  free_graph(g);
  
  remove_transitivity_edges_weight_conservative(gg);
  fprintf(stdout,"\nGraph before sp-zation:\n");
  print_graph_to_dot_file(gg,stdout);

  graph_t *sp_ized_g = graph_sp_ization(gg);
  free_graph(gg);
  
  
  fprintf(stdout,"\nGraph after sp-zation:\n");
  print_graph_to_dot_file(sp_ized_g,stdout);

  /* Test removed as the output depends on the version of the igraph library */
  /* fprintf(stdout,"\nSame graph converted to igraph:\n"); */
  /* igraph_t igraph = convert_to_igraph(sp_ized_g, NULL, NULL, NULL); */
  /* igraph_write_graph_dot(&igraph, stdout); */
  /* igraph_destroy(&igraph); */
  
  enforce_single_source_and_target(sp_ized_g);
  remove_vertex(sp_ized_g,sp_ized_g->target);
  enforce_single_source_and_target(sp_ized_g);
  remove_vertex(sp_ized_g,sp_ized_g->source);
  remove_vertex(sp_ized_g,sp_ized_g->target);
  enforce_single_source_and_target(sp_ized_g);
  remove_vertex(sp_ized_g,sp_ized_g->source);
  remove_vertex(sp_ized_g,sp_ized_g->target);

  fprintf(stdout,"\nSP-ized graph after removing (copies of) v1, v2, v4, v5 and SYNC..:\n");

  print_graph_to_dot_file(sp_ized_g,stdout);
  enforce_single_source_and_target(sp_ized_g);

  fprintf(stdout,"\nAnd after enforcing single source/target:\n");
  print_graph_to_dot_file(sp_ized_g,stdout);
  free(sp_ized_g);
}


int main(int argc, char **argv) {

  simple_graph_test();
  return 0;
}
