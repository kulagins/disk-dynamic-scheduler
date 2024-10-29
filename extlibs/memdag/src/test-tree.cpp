#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "graph.hpp"
#include "tree.hpp"


  


void check_tree_equality(tree_t treea, tree_t treeb) {
  if ((treea->out_size != treeb->out_size)
      || (treea->time != treeb->time)
      || (treea->nb_of_children != treeb->nb_of_children)
      || (treea->subtree_size != treeb->subtree_size)
      || (treea->subtree_time != treeb->subtree_time)
      || (treea->total_input_size != treeb->total_input_size)
      || (treea->max_subtree_critical_path != treeb->max_subtree_critical_path)) {
    fprintf(stderr,"ERROR, trees differ.\n");
    assert(0);
  }
}


void simple_tree_test(const char *filename) {
  tree_t tree = read_tree_as_node_list(filename);
  tree_t tree2 = copy_tree(tree);
  check_tree_equality(tree,tree2);
  free_tree(tree);

  graph_t *graph = tree_to_graph(tree2);
  tree_t tree3 = graph_to_tree(graph);
  if (!tree3) {
    fprintf(stderr,"graph_to_tree: not a tree\n");
    exit(2);
  }
  check_tree_equality(tree2, tree3);

  free(tree3);
  
  print_tree_as_node_list(stdout,tree2);
  print_dot_tree(stdout,tree2);

  fprintf(stdout,"OPT memory:%f\nSchedule: ", Liu_optimal_sequential_memory(tree2,NULL));
  int *opt_schedule;
  Liu_optimal_sequential_memory(tree2,&opt_schedule);
  for(int i=0;i<tree2->subtree_size; i++) {
    fprintf(stdout,"%d (%s)%s", opt_schedule[i],
	    (tree2[opt_schedule[i]].name?tree2[opt_schedule[i]].name:""),
	    ((i==tree2->subtree_size - 1)?"\n":", "));
  }


  fprintf(stdout,"PostOrder memory:%f\nSchedule: ", best_postorder_sequential_memory(tree2,NULL));
  int *po_schedule;
  best_postorder_sequential_memory(tree2,&po_schedule);
  for(int i=0;i<tree2->subtree_size; i++) {
    fprintf(stdout,"%d (%s)%s", po_schedule[i],
	    (tree2[po_schedule[i]].name?tree2[po_schedule[i]].name:""),
	    ((i==tree2->subtree_size - 1)?"\n":", "));
  }

  
}


int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr,"ERROR: expected 1 parameter: <tree file name>\n");
    exit(1);
  }
  const char *filename = argv[1];
  simple_tree_test(filename);
  return 0;
}
