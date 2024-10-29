#include <stdlib.h>
#include <stdio.h>
//#include <string.h>
#include "fifo.hpp"
#include "graph.hpp"
#include "sp-graph.hpp"


int main(int argc, char **argv) {



  if (argc != 2) {
    fprintf(stderr,"Usage %s <dot-graph>\n", argv[0]);
    return 1;
  }

  graph_t * graph = read_dot_graph(argv[1], "weight", NULL, NULL);

  enforce_single_source_and_target(graph);
  SP_tree_t *tree = build_SP_decomposition_tree(graph);
  
  if (tree) {
    fprintf(stdout,"This is a SP graph, corresponding decomposition tree:\n");
    graph_t *tree_graph = SP_tree_to_graph(tree);
    print_graph_to_dot_file(tree_graph, stdout);
    free_graph(tree_graph);
    
    vertex_t **schedule = compute_optimal_SP_traversal(graph, tree);
    fprintf(stdout,"optimal schedule:\n");
    for(int i=0; i<graph->number_of_vertices; i++)
      fprintf(stdout,"%s%s",(i==0)?"":", ", schedule[i]->name);
    fprintf(stdout,"\n");
    fprintf(stdout,"Peak memory: %f\n", compute_peak_memory(graph, schedule));
    SP_tree_free(tree);
  } else {
    fprintf(stdout,"This is a NOT a SP graph.\n");
  }

  graph_t *sp_ized_graph = graph_sp_ization(graph);
  fprintf(stdout,"Graph after SP_ization:\n");
  print_graph_to_dot_file(sp_ized_graph, stdout);
  compute_bottom_and_top_levels(graph);
  compute_bottom_and_top_levels(sp_ized_graph);
  fprintf(stdout,"Critical path of original graph: %e  of SP-ized graph: %e   ratio:%e\n",
	  graph->source->bottom_level, sp_ized_graph->source->bottom_level, sp_ized_graph->source->bottom_level /graph->source->bottom_level);
  
  free_graph(sp_ized_graph);
  free_graph(graph);
    
  return 0;
}
