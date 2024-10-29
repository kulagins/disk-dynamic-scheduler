#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "float.h"
#include "fifo.hpp"
#include "graph.hpp"
#include "sp-graph.hpp"

/** \file sp-traversals.c
 * \brief Compute minimum memory traversal for Series-Parallel graphs
 */


//#define DEBUG_SP_TRAVERSAL
///@private
typedef struct item {
  vertex_t *vertex;
  double mem_increment;
  struct item *next, *prev;
} item_t;

///@private

typedef struct schedule {
  item_t *first_item, *last_item;         
} schedule_t;

///@private

static void free_schedule(schedule_t schedule) {
  item_t *next = NULL;
  for(item_t *item=schedule.first_item; item; item=next) {
    next = item->next;
    free(item);
  }
}

// TODO later: avoid reversing schedules by implementing separate merge for out-trees
///@private
static schedule_t reverse_schedule(schedule_t schedule) {
  item_t *orig_first = schedule.first_item;
  item_t *orig_last = schedule.last_item;
  
  item_t *next=NULL;
  for(item_t *item=orig_first; item; item=next) {
    next = item->next;
    item->next = item->prev;
    item->prev = next;
    item->mem_increment = -item->mem_increment;
  }
  schedule.first_item = orig_last;
  schedule.last_item = orig_first;
  return schedule;
}

///@private
static schedule_t single_vertex_schedule(vertex_t *vertex, double mem_increment) {
  item_t *item = (item_t*) calloc(1, sizeof(item_t));
  item->vertex = vertex;
  item->mem_increment = mem_increment;
  item->next = NULL;
  item->prev = NULL;
  
  schedule_t schedule;
  schedule.first_item = item;
  schedule.last_item = item;
  
  return schedule;
}

///@private
static inline schedule_t empty_schedule(void) {
  return (schedule_t) {NULL, NULL};
}

///@private
static void extend_schedule(schedule_t *s1, schedule_t s2) {
  if (s1->first_item==NULL) {
    s1->first_item = s2.first_item;
    s1->last_item = s2.last_item;
  } else if (s2.first_item != NULL) {
    s1->last_item->next = s2.first_item;
    s2.first_item->prev = s1->last_item;
    s1->last_item = s2.last_item;
  }
#ifdef DEBUG_SP_TRAVERSAL
  /* fprintf(stderr,"Schedule after extension:"); */
  /* for(item_t *item=s1->first_item; item; item=item->next) */
  /*   fprintf(stderr," %s(%d, m:%f)", item->vertex->name, item->vertex->id, item->mem_increment); */
  /* fprintf(stderr, "\n"); */
#endif
}

///@private
typedef struct segment {
  item_t *first_item, *last_item;
  double hill, valley;
  struct segment *next_segment;
} segment_t;

///@private
static schedule_t parallel_merge_for_in_tree(schedule_t *schedules, int nb_of_branches) {
  segment_t **segments = (segment_t**) calloc(nb_of_branches, sizeof(segment_t*));

#ifdef DEBUG_SP_TRAVERSAL
  fprintf(stderr, "\nDecomposition into segments\n");
#endif
						       
  for(int branch=0;  branch<nb_of_branches; branch++) {
    item_t *first_item = schedules[branch].first_item;
    double current_memory = 0; 
    segment_t *last_segment = NULL;
#ifdef DEBUG_SP_TRAVERSAL
    fprintf(stderr, "Segments for branch %d:", branch);
#endif
    while (first_item) {
      // search for next maximum (hill)
      double hill_value = 0; 
      item_t *hill_pos = NULL;
      for(item_t *item=first_item; item; item=item->next) {
	current_memory += item->mem_increment;
	if (current_memory >= hill_value) {
	  hill_value = current_memory;
	  hill_pos = item;
	}
      }
      assert(hill_pos);
      // search for next minimum after (or at) the hill (valley)
      double valley_value = hill_value;
      item_t *valley_pos = hill_pos;
      current_memory = hill_value;
      for(item_t *item=hill_pos->next; item; item=item->next) {
	current_memory += item->mem_increment;
	if (current_memory <= valley_value) {
	  valley_value = current_memory;
	  valley_pos = item;
	}
      }
#ifdef DEBUG_SP_TRAVERSAL
      fprintf(stderr,"[%s(%d) -> %s(%d) H=%f on %s(%d) V=%f]",
	      first_item->vertex->name,	first_item->vertex->id, valley_pos->vertex->name, valley_pos->vertex->id,
	      hill_value, hill_pos->vertex->name, hill_pos->vertex->id, valley_value);
#endif

      // record new segment
      segment_t *new_segment = (segment_t*) calloc(1, sizeof(segment_t));
      new_segment->first_item = first_item;
      new_segment->last_item = valley_pos;
      new_segment->hill = hill_value;
      new_segment->valley = valley_value;
      if (last_segment) {
	last_segment->next_segment = new_segment;
      } else {
	segments[branch] = new_segment;
      }
      last_segment = new_segment;

      // prepare to start again
      first_item = valley_pos->next;
      current_memory = valley_value;
    }
#ifdef DEBUG_SP_TRAVERSAL
    fprintf(stderr,"\n");
#endif
  }

  // Merge segments, choosing the one with maximum hill-valley
  schedule_t final_schedule = empty_schedule();
  int nb_of_remaining_branches = 0;
  for(int branch=0;  branch<nb_of_branches; branch++) 
    if (segments[branch])
      nb_of_remaining_branches++;
  while (nb_of_remaining_branches>0) {
    int best_branch = -1;
    double best_score = 0;
    for(int i=0; i<nb_of_branches; i++) {
      if (segments[i]) {
	double new_score =  segments[i]->hill - segments[i]->valley;
	if ((best_branch==-1) || (best_score < new_score)) {
	  best_branch = i;
	  best_score = new_score;
	}
      }
    }
    // copy segment into final schedule
    segment_t *best_segment = segments[best_branch];
    extend_schedule(&final_schedule, (schedule_t) {best_segment->first_item, best_segment->last_item});

    // update segments and nb_of_remaining_branches, free segment
    segments[best_branch] = best_segment->next_segment;
    free(best_segment);
    if (segments[best_branch]==NULL)
      nb_of_remaining_branches--;
  }
  return final_schedule;
}

///@private
typedef struct liu_res {
  schedule_t schedule_before_min_cut; // schedule from the source (excluded) to the min-cut
  schedule_t schedule_after_min_cut;  // schedule from the min-cut to the target (excluded)
  double min_cut_value; // value of the min-cut (used to choose best cut in parallel composition)
  double initial_memory; // what is in the memory in the beginning of the schedule, i.e., weight of the source output(s)
  double final_memory;  // what is in the memory at the end of the schedule, i.e., weight fo the target input(s)
} liu_res_t;

///@private
static liu_res_t compute_optimal_SP_traversal_rec(graph_t *sp_graph, SP_tree_t *tree) {
#ifdef DEBUG_SP_TRAVERSAL
  fprintf(stderr,"\n*** Entering tree node %d (%s)\n",tree->id, COMPOTYPE2STR(tree->type));
#endif
  /*
   * Base case: single edge
   */
  if (tree->nb_of_children == 0) {
    edge_t *edge = find_edge(tree->source, tree->target);
    assert(edge);
#ifdef DEBUG_SP_TRAVERSAL
    fprintf(stderr,"Base case of with edge weight %f\n",edge->weight);
#endif
    return (liu_res_t) {empty_schedule(), empty_schedule(), edge->weight, edge->weight, edge->weight};
  }

  liu_res_t *sub_results = (liu_res_t*) calloc(tree->nb_of_children, sizeof(liu_res_t));
  for(int i=0; i<tree->nb_of_children; i++)
    sub_results[i] = compute_optimal_SP_traversal_rec(sp_graph, tree->children[i]);
  liu_res_t result;
  
  if (tree->type == SP_SERIES_COMP) {
    for(int i=0; i<tree->nb_of_children; i++) {
#ifdef DEBUG_SP_TRAVERSAL
      fprintf(stderr,"i=%d ",i);
#endif
      if (i==0) {
	result = sub_results[i];  // Warning, everything is copied even the initial_memory and final_memory
      } else {
	double mem_increment = sub_results[i].initial_memory - result.final_memory;
	schedule_t schedule_source = single_vertex_schedule(tree->children[i]->source, mem_increment);
	
	if (sub_results[i].min_cut_value < result.min_cut_value) { // New minimum cut in current sub_result, extend schedule before min cut
#ifdef DEBUG_SP_TRAVERSAL
	  fprintf(stderr,"case 1\n");
#endif
	  result.min_cut_value = sub_results[i].min_cut_value;
	  extend_schedule(&result.schedule_before_min_cut, result.schedule_after_min_cut);
	  extend_schedule(&result.schedule_before_min_cut, schedule_source);
	  extend_schedule(&result.schedule_before_min_cut, sub_results[i].schedule_before_min_cut);
	  result.schedule_after_min_cut = sub_results[i].schedule_after_min_cut;
	} else { // Keep current minimum, add new source and new both new schedules at the end
#ifdef DEBUG_SP_TRAVERSAL
	  fprintf(stderr,"case 2\n");
#endif
	  extend_schedule(&result.schedule_after_min_cut, schedule_source);
	  extend_schedule(&result.schedule_after_min_cut, sub_results[i].schedule_before_min_cut);
	  extend_schedule(&result.schedule_after_min_cut, sub_results[i].schedule_after_min_cut);
	}
	result.final_memory = sub_results[i].final_memory;
      }
    }
  } else {
    assert(tree->type == SP_PARALLEL_COMP);
    schedule_t *reverse_schedules_before_min_cut = (schedule_t*) calloc(tree->nb_of_children, sizeof(schedule_t));
    schedule_t *schedules_after_min_cut = (schedule_t*) calloc(tree->nb_of_children, sizeof(schedule_t));
    result.min_cut_value = 0.0;
    result.initial_memory = 0.0;
    result.final_memory = 0.0;
    for(int i=0; i<tree->nb_of_children; i++) {
      reverse_schedules_before_min_cut[i] = reverse_schedule(sub_results[i].schedule_before_min_cut);
      schedules_after_min_cut[i] = sub_results[i].schedule_after_min_cut;

      result.min_cut_value += sub_results[i].min_cut_value;
      result.initial_memory += sub_results[i].initial_memory;
      result.final_memory += sub_results[i].final_memory;      
    }


#ifdef DEBUG_SP_TRAVERSAL
  fprintf(stderr,"\n*** Parallel merge after min-cut at node %d (%s)\n",tree->id, COMPOTYPE2STR(tree->type));
#endif
    result.schedule_after_min_cut = parallel_merge_for_in_tree(schedules_after_min_cut, tree->nb_of_children);
#ifdef DEBUG_SP_TRAVERSAL
  fprintf(stderr,"\n*** Parallel merge before min-cut at node %d (%s)\n",tree->id, COMPOTYPE2STR(tree->type));
#endif
    schedule_t tmp_schedule = parallel_merge_for_in_tree(reverse_schedules_before_min_cut, tree->nb_of_children);
    result.schedule_before_min_cut = reverse_schedule(tmp_schedule);
  }
  free(sub_results);

#ifdef DEBUG_SP_TRAVERSAL
  fprintf(stderr,"\n*** Result for tree node %d (%s):\n",tree->id, COMPOTYPE2STR(tree->type));
  fprintf(stderr,"Schedule before min cut:");
  for(item_t *item=result.schedule_before_min_cut.first_item; item; item=item->next)
    fprintf(stderr," %s(%d)", item->vertex->name, item->vertex->id);
  fprintf(stderr,"\nSchedule atfer min cut:");
  for(item_t *item=result.schedule_after_min_cut.first_item; item; item=item->next)
    fprintf(stderr," %s(%d)", item->vertex->name, item->vertex->id);
  fprintf(stderr,"\nMin cut value: %f\n", result.min_cut_value);
  fprintf(stderr,"Initial memory: %f\n",    result.initial_memory);	  
  fprintf(stderr,"Final memory: %f\n",    result.final_memory);	  
#endif

  
  return result;
}

/**
 * Compute the sequential schedule of an SP graph which needs minimum memory.
 *
 * @param sp_graph the initial Series-Parallel graph
 * @param tree the decomposition tree obtained with build_SP_decomposition_tree()
 */

vertex_t **compute_optimal_SP_traversal(graph_t  *sp_graph, SP_tree_t *tree) { 
  vertex_t **final_schedule = (vertex_t**) calloc(sp_graph->number_of_vertices, sizeof(vertex_t*));
  liu_res_t rec_result = compute_optimal_SP_traversal_rec(sp_graph, tree);
  final_schedule[0] = sp_graph->source;
  //fprintf(stderr,"copying node 0/%d: %s\n", sp_graph->number_of_vertices, sp_graph->source->name);
  int next_index = 1;
  for(item_t *item = rec_result.schedule_before_min_cut.first_item; item; item=item->next) {
    final_schedule[next_index++] = item->vertex;
    //fprintf(stderr,"copying node %d/%d: %s\n",next_index-1, sp_graph->number_of_vertices, item->vertex->name);
  }
  for(item_t *item=rec_result.schedule_after_min_cut.first_item; item; item=item->next) {
    final_schedule[next_index++] = item->vertex;
    //fprintf(stderr,"copying node %d/%d: %s\n",next_index-1, sp_graph->number_of_vertices, item->vertex->name);
  }
  final_schedule[next_index++] = sp_graph->target;
  //fprintf(stderr,"copying node %d/%d: %s\n",next_index-1, sp_graph->number_of_vertices, sp_graph->target->name);
  assert(next_index==sp_graph->number_of_vertices);

  free_schedule(rec_result.schedule_before_min_cut);
  free_schedule(rec_result.schedule_after_min_cut);
  
  return final_schedule;
}

