#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "tree.hpp"

//#define DEBUG_LIU_OPT_SEQ(command) {command;}
#define DEBUG_LIU_OPT_SEQ(command) {}


typedef struct s_Liu_schedule_t {
  int *node_list;
  int nb_of_nodes;
  int nb_of_segments;
  int *segment_start;
  double *segment_peak;
  double *segment_valley;
} *Liu_schedule_t;

void free_Liu_schedule(Liu_schedule_t schedule, int produce_schedule) {
  if (produce_schedule==1) {
    free(schedule->node_list);
    free(schedule->segment_start);
  }
  free(schedule->segment_peak);
  free(schedule->segment_valley);
  free(schedule);
}

void Liu_merge_last_two_segments_if_needed(Liu_schedule_t schedule, int *last_segment) {
  while ((*last_segment != 0) &&
	 ((schedule->segment_peak[*last_segment] >= schedule->segment_peak[(*last_segment) - 1])
	  || (schedule->segment_valley[*last_segment] <= schedule->segment_valley[(*last_segment) - 1]))) {
    schedule->segment_valley[(*last_segment) - 1] =    schedule->segment_valley[*last_segment];
    schedule->segment_peak[(*last_segment) - 1] = max_memdag(schedule->segment_peak[(*last_segment) - 1], schedule->segment_peak[*last_segment]);
    *last_segment = (*last_segment) - 1;
    schedule->nb_of_segments--;
    DEBUG_LIU_OPT_SEQ(fprintf(stderr,"Merged last segment with the previous one (new peak:%.1f, new valley:%.1f)\n",
			      schedule->segment_peak[*last_segment],schedule->segment_valley[*last_segment]));
  }
}

void print_Liu_schedule(FILE *out, Liu_schedule_t schedule, int produce_schedule) {
  int i;
  if (produce_schedule==1) {
    fprintf(out,"node list: ");
    for(i=0; i<schedule->nb_of_nodes; i++)
      fprintf(out,"%d%s",schedule->node_list[i], ((i==schedule->nb_of_nodes-1)?"\t\t":","));
    fprintf(out,"segment start [peak,valley]: ");
    for(i=0; i<schedule->nb_of_segments; i++)
      fprintf(out,"%d[%.1f,%.1f]%s",schedule->segment_start[i],schedule->segment_peak[i],schedule->segment_valley[i], (i==schedule->nb_of_segments-1)?"\n":",");
  } else {
    fprintf(out,"segment [peak,valley]: ");
    for(i=0; i<schedule->nb_of_segments; i++)
      fprintf(out,"[%.1f,%.1f]%s",schedule->segment_peak[i],schedule->segment_valley[i], (i==schedule->nb_of_segments-1)?"\n":",");
  }
}


Liu_schedule_t Liu_sequential_optimal_schedule(tree_t tree, int produce_schedule) {
  int k;

  // 1. Recursively compute the optimal schedules
  Liu_schedule_t *sub_schedules = newn(Liu_schedule_t, tree->nb_of_children);
  int total_nb_of_segments = 0;

  DEBUG_LIU_OPT_SEQ(fprintf(stderr,"\n******* Liu_sequential_optimal_schedule in node %d\n", tree->id));

  for(k=0; k<tree->nb_of_children; k++) {
    sub_schedules[k] = Liu_sequential_optimal_schedule(tree->children[k], produce_schedule);
    if (produce_schedule) {
      assert(sub_schedules[k]->nb_of_nodes == tree->children[k]->subtree_size);
    }
    DEBUG_LIU_OPT_SEQ(fprintf(stderr,"******** (%d) schedule from child %d:", tree->id, tree->children[k]->id));
    DEBUG_LIU_OPT_SEQ(print_Liu_schedule(stderr,sub_schedules[k], produce_schedule));

    total_nb_of_segments += sub_schedules[k]->nb_of_segments;
  }

  // 2. Merge and recompute segments
  int *next_segment_of_child = newn(int, tree->nb_of_children);

  // First allocate total_nb_of_segments+1 segments (the +1 is for the root) and then reallocate
  Liu_schedule_t schedule = newn(struct s_Liu_schedule_t, 1);
  if (produce_schedule) {
    schedule->node_list = newn(int, tree->subtree_size);
    schedule->nb_of_nodes = tree->subtree_size;
  }
  int next_node_in_list = 0;
  int next_segment = 0;

  //  int max_used_segments = 0;
  schedule->nb_of_segments = 0;
  if (produce_schedule) {
    schedule->segment_start = newn(int, total_nb_of_segments + 1);
  }
  schedule->segment_peak = newn(double, total_nb_of_segments + 1);
  schedule->segment_valley = newn(double, total_nb_of_segments + 1);

  double current_residual_memory = 0;

  int remaining_nonempty_sub_schedules = tree->nb_of_children;
  while (remaining_nonempty_sub_schedules > 0) {
    int chosen_child = -1;
    double best_peak_valley_value = 0;
    // find the segment with largest peak-valley value
    for(k=0; k<tree->nb_of_children; k++) {
      if (next_segment_of_child[k] < sub_schedules[k]->nb_of_segments) {
	double local_peak_valley_value = sub_schedules[k]->segment_peak[next_segment_of_child[k]] - sub_schedules[k]->segment_valley[next_segment_of_child[k]];
	//	fprintf(stderr,"Considering child #%d with peak-valley=%.1f\n",k,local_peak_valley_value);
	if ((chosen_child < 0) || (best_peak_valley_value < local_peak_valley_value)) {
	  best_peak_valley_value = local_peak_valley_value;
	  chosen_child = k;
	  // fprintf(stderr, "Updated chosen_child to %d\n",chosen_child);
	}
      }
    }


    DEBUG_LIU_OPT_SEQ(fprintf(stderr,"Combine at node %d: selected segment %d from child %d(#%d) (peak:%.1f valley:%.1f): [",
			      tree->id, next_segment_of_child[chosen_child], tree->children[chosen_child]->id, chosen_child,
			      sub_schedules[chosen_child]->segment_peak[next_segment_of_child[chosen_child]],
			      sub_schedules[chosen_child]->segment_valley[next_segment_of_child[chosen_child]]));


    if (produce_schedule) {
      // copy the segment next_segment_of_child[chosen_child] to the schedule in a new segment
      int *chosen_node_list = sub_schedules[chosen_child]->node_list;
      int chosen_start_index = sub_schedules[chosen_child]->segment_start[next_segment_of_child[chosen_child]];
      int chosen_stop_index;
      if (next_segment_of_child[chosen_child] < (sub_schedules[chosen_child]->nb_of_segments-1)) {
	chosen_stop_index = sub_schedules[chosen_child]->segment_start[next_segment_of_child[chosen_child]+1] - 1;
      } else {
	chosen_stop_index = sub_schedules[chosen_child]->nb_of_nodes - 1;
      }

      int i;
      schedule->segment_start[next_segment] = next_node_in_list;
      for(i=0; i <= chosen_stop_index - chosen_start_index; i++) {
	//      fprintf(stderr,"about to write %d at node_list[%d/%d]\n", chosen_node_list[chosen_start_index + i], next_node_in_list, tree->subtree_size);
	assert(chosen_start_index < sub_schedules[chosen_child]->nb_of_nodes);
	assert(next_node_in_list < tree->subtree_size);
	schedule->node_list[next_node_in_list++] = chosen_node_list[chosen_start_index + i];
	DEBUG_LIU_OPT_SEQ(fprintf(stderr,"%d%s",chosen_node_list[chosen_start_index + i], ((i == chosen_stop_index)?"]\n":",")));
      }
    }
    // Compute peak and valley of the new segment:
    if (next_segment_of_child[chosen_child] == 0) {
      DEBUG_LIU_OPT_SEQ(fprintf(stderr,"  case 1 %.1f +%.1f",
				current_residual_memory,sub_schedules[chosen_child]->segment_valley[next_segment_of_child[chosen_child]]));
      schedule->segment_peak[next_segment] = current_residual_memory + sub_schedules[chosen_child]->segment_peak[next_segment_of_child[chosen_child]];
      schedule->segment_valley[next_segment] = current_residual_memory + sub_schedules[chosen_child]->segment_valley[next_segment_of_child[chosen_child]];
    } else{
      DEBUG_LIU_OPT_SEQ(fprintf(stderr,"  case 2"));
      schedule->segment_peak[next_segment] = current_residual_memory
	+ sub_schedules[chosen_child]->segment_peak[next_segment_of_child[chosen_child]]
	- sub_schedules[chosen_child]->segment_valley[next_segment_of_child[chosen_child]-1];
      schedule->segment_valley[next_segment] = current_residual_memory
	+ sub_schedules[chosen_child]->segment_valley[next_segment_of_child[chosen_child]]
	- sub_schedules[chosen_child]->segment_valley[next_segment_of_child[chosen_child]-1];
    }
    current_residual_memory = schedule->segment_valley[next_segment];
    DEBUG_LIU_OPT_SEQ(fprintf(stderr,"     segment peak:%.1f  valley:%.1f\n", schedule->segment_peak[next_segment], schedule->segment_valley[next_segment]));

    //    max_used_segments = max(max_used_segments, next_segment + 1);
    Liu_merge_last_two_segments_if_needed(schedule, &next_segment);
    next_segment++;
    next_segment_of_child[chosen_child]++;
    if (next_segment_of_child[chosen_child] == sub_schedules[chosen_child]->nb_of_segments) {
      remaining_nonempty_sub_schedules--;
    }
  }
  // Add the root
  if (produce_schedule) {
    DEBUG_LIU_OPT_SEQ(fprintf(stderr,"about to write %d at node_list[%d/%d]\n", tree->id, next_node_in_list, tree->subtree_size));
    schedule->segment_start[next_segment] = next_node_in_list;
    schedule->node_list[next_node_in_list++] = tree->id;
  }
  schedule->segment_peak[next_segment] = max_memdag(current_residual_memory,
					     current_residual_memory - tree->total_input_size + tree->out_size); // using Liu's model: mem_peak=max(sum inputs, outputs)
  schedule->segment_valley[next_segment] = current_residual_memory + tree->out_size - tree->total_input_size;
  DEBUG_LIU_OPT_SEQ(fprintf(stderr,"Added root node %d as a segment (peak:%.1f valley:%.1f)\n",
			    tree->id, schedule->segment_peak[next_segment], schedule->segment_valley[next_segment]));
  //  max_used_segments = max(max_used_segments, next_segment + 1);
  Liu_merge_last_two_segments_if_needed(schedule, &next_segment);
  next_segment++;

  /* if (schedule->segment_peak[0] > tree->PO_subtree_mem) { */
  /*   fprintf(stderr,"Error: in node %d, Liu opt uses more memory (%.1f) than best PostOrder (%.1f)\n", tree->id, schedule->segment_peak[0],tree->PO_subtree_mem); */
  /*   fprintf(stderr,"PostOrder order for each child: "); */
  /*   int i; */
  /*   for(i=0; i<tree->nb_of_children; i++) { fprintf(stderr," %d(%d)", tree->children[i]->id, tree->children[i]->PO_subtree_order); } */
  /*   fprintf(stderr,"\n"); */
  /*   exit(2); */
  /* } */


  //Reallocate data structures with nb_of_segments = next_segment
  assert(next_segment <= total_nb_of_segments + 1);
  schedule->nb_of_segments = next_segment;

  //  fprintf(stderr,"%d -> %d max %d\n", total_nb_of_segments + 1, schedule->nb_of_segments, max_used_segments);


  if (produce_schedule) {
    schedule->segment_start = (int*) realloc(schedule->segment_start, next_segment * sizeof(int));
  }
  schedule->segment_peak = (double*) realloc(schedule->segment_peak, next_segment * sizeof(double));
  schedule->segment_valley = (double*) realloc(schedule->segment_valley, next_segment * sizeof(double));

  for(k=0; k<tree->nb_of_children; k++) {
    free_Liu_schedule(sub_schedules[k], produce_schedule);
  }
  free(sub_schedules);
  free(next_segment_of_child);
  DEBUG_LIU_OPT_SEQ(fprintf(stderr,"Schedule at node %d: %d segments\n\n",tree->id, schedule->nb_of_segments));

  return schedule;
}


/****
 * Computes the memory-optimal schedule of the given tree, using Liu's algorithm and returns its peak memory.
 * If schedule is not NULL, an array is allocated and filled with the nodes of the tree in the order of the optimal schedule.
 */

double Liu_optimal_sequential_memory(tree_t tree, int **schedule) {
  int produce_schedule = (schedule != NULL);
  Liu_schedule_t Liu_schedule = Liu_sequential_optimal_schedule(tree, produce_schedule);
  double optimal_sequential_memory = Liu_schedule->segment_peak[0];
  if (produce_schedule) {
    *schedule = Liu_schedule->node_list;
      // do not free node_list !
  } else {
    free(Liu_schedule->node_list);
  }
  free(Liu_schedule->segment_start);
  free(Liu_schedule->segment_peak);
  free(Liu_schedule->segment_valley);
  free(Liu_schedule);
  return optimal_sequential_memory;
}

