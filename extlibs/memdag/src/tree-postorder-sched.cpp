#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#include "tree.hpp"

//#define DEBUG_SEQ_PO(command) {command;}
#define DEBUG_SEQ_PO(command) {}

/******************************************************
 ***     postorder schedule to minimize memory      ***
 ******************************************************/

static double *local_children_memory_peak = NULL;
static double *local_children_residual_memory = NULL;

int cmp_postorder_subtrees(const void* pi, const void *pj) { 
  int i = *(int*)pi;
  int j = *(int*)pj;
  double diff =
    (local_children_memory_peak[j] - local_children_residual_memory[j])
    - (local_children_memory_peak[i] - local_children_residual_memory[i]);
  //      fprintf(stderr,"comparing subtrees %d and %d, diff: %.2f\n", i, j, diff);
  return sign(diff);
}
    


/****
 * Computes the postorder schedule of the given tree minimizing the memory, using Liu's algorithm and returns its peak memory.
 * If schedule is not NULL, an array is allocated and filled with the nodes of the tree in the order of the optimal schedule.
 */


double best_postorder_sequential_memory(tree_t tree, int **schedule) {

  if (tree->nb_of_children == 0) {
    if (schedule) {
      int *my_schedule = newn(int, 1);
      my_schedule[0] = tree->id;
      *schedule = my_schedule;
    }
    double max_mem;
    max_mem = tree->out_size;
    return max_mem;
  } else {
    double *children_memory_peak;
    children_memory_peak = newn(double, tree->nb_of_children);
    assert(children_memory_peak);
    int k;
    int **children_schedules;
    if (schedule) {
      children_schedules = newn(int*, tree->nb_of_children);
    }    
    for(k=0; k<tree->nb_of_children; k++) {
      if (schedule) {
	children_memory_peak[k] = best_postorder_sequential_memory(tree->children[k], &(children_schedules[k]));
      } else {
	children_memory_peak[k] = best_postorder_sequential_memory(tree->children[k], NULL);
      }
      
      DEBUG_SEQ_PO(if (schedule) { int i;fprintf(stderr,"schedule received from child %d: ", tree->children[k]->id);
	  for(i=0; i<tree->children[k]->subtree_size; i++) {fprintf(stderr,"%d ",children_schedules[k][i]);}
	  fprintf(stderr,"\n");	});    
    }

    int *sorted_children_indices     = newn(int,    tree->nb_of_children);
    assert(sorted_children_indices);
    double *children_residual_memory = newn(double, tree->nb_of_children);
    assert(children_residual_memory);
    for(k=0; k<tree->nb_of_children; k++) {
      sorted_children_indices[k] = k;
      children_residual_memory[k] = tree->children[k]->out_size;
    }
    
    DEBUG_SEQ_PO(fprintf(stderr, "Seq-PO at node %d, children:\n", tree->id);
		 for(k=0; k<tree->nb_of_children; k++) {
		   fprintf(stderr, "child %d(%d), peak=%.2f, residual=%.2f, diff=%.2f\n", 
			   k, tree->children[k]->id, children_memory_peak[k], children_residual_memory[k], children_memory_peak[k] - children_residual_memory[k]);
		 });    
    
    
    local_children_memory_peak = children_memory_peak;
    local_children_residual_memory =children_residual_memory;

    qsort(sorted_children_indices, tree->nb_of_children, sizeof(int), cmp_postorder_subtrees);

    DEBUG_SEQ_PO(fprintf(stderr, "Seq-PO at node %d, sorted children: ", tree->id);
		 for(k=0; k<tree->nb_of_children; k++) {
		   fprintf(stderr,"%s%d(%d)",(k?", ":""), sorted_children_indices[k], tree->children[sorted_children_indices[k]]->id);
		 }
		 fprintf(stderr,"\n"));
   

    double max_mem = 0;
    double sum_of_residual = 0;
    int *my_schedule;
    int next_schedule_index;

    if (schedule) {
      my_schedule = newn(int, tree->subtree_size); 
      next_schedule_index = 0;
    }
    for(k=0; k<tree->nb_of_children; k++) {
      int selected_child = sorted_children_indices[k];
      max_mem = max_memdag(max_mem, sum_of_residual + children_memory_peak[selected_child]);
      sum_of_residual += children_residual_memory[selected_child];
      if (schedule) {
	memcpy(my_schedule + next_schedule_index, children_schedules[selected_child], tree->children[selected_child]->subtree_size * sizeof(int));
	next_schedule_index += tree->children[selected_child]->subtree_size;
	free(children_schedules[selected_child]);
      }
    }
    max_mem = max_memdag(max_mem, max_memdag(sum_of_residual, tree->out_size));
    if (schedule) {
      my_schedule[next_schedule_index] = tree->id;
      *schedule = my_schedule;
    }

    DEBUG_SEQ_PO(if (schedule) {
	int i;
	fprintf(stderr,"schedule after PO-Seq at node %d: ", tree->id);
	for(i=0; i<tree->subtree_size; i++) {
	  fprintf(stderr,"%d ",my_schedule[i]);
	}
	fprintf(stderr,"\n");
      });

    free(sorted_children_indices);
    free(children_memory_peak);
    free(children_residual_memory);
    if (schedule) {
      free(children_schedules);
    }
    return max_mem;
  }
}
