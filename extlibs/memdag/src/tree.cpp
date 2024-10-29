#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <math.h>

#include "tree.hpp"

#define DEBUG_TREES(command) {}
//#define DEBUG_TREES(command) {command;}


///@private
void update_subtree_stats(tree_t tree) {
  //  fprintf(stderr,"compute_subtree_times in node %d\n",tree->id);
  double local_subtree_time = tree->time;
  int local_subtree_size = 1;
  double local_total_inputs = 0.0;
  
  int k;
  tree->critical_path = tree->time;
  if (tree->parent) {
    tree->critical_path += tree->parent->critical_path;
  }

  for(k=0; k<tree->nb_of_children; k++) {
    update_subtree_stats(tree->children[k]);
    local_subtree_time += tree->children[k]->subtree_time;
    local_subtree_size += tree->children[k]->subtree_size;
    local_total_inputs += tree->children[k]->out_size;
  }
  tree->subtree_time = local_subtree_time;
  tree->subtree_size = local_subtree_size;
  tree->total_input_size = local_total_inputs;
}




tree_t read_tree_as_node_list(const char *filename) {
  int max_id = 0;
  int id;
  int parent;
  long long out_size, processing_size, time;
  char line[255];
  int use_processing_size = -1;

  ////////// NB: tree format:
  /// id parent_id ni fi wi
  /// id=1...N
  /// parent_id==0 means this is the root

  ///////////////////////// STEP 1: count the number of nodes and allocate the tree
  FILE *input = fopen(filename, "r");
  if (!input) {
    fprintf(stderr,"Unable to open file %s\n", filename);
    abort();
  }
  do {
    fgets(line, 255, input);
  } while (line[0] == '%');
  int r;
  while ((r=sscanf(line,"%d %d %lld %lld %lld\n",&id, &parent, &processing_size, &out_size, &time)) >= 4) {
    if ((r==4) && (use_processing_size==1)) {
      fprintf(stderr,"ERROR: Incorrect format: 4 elements while expecting 5 with (use_processing_size=1)\n");
      exit(1);
    }
    if ((r==5) && (use_processing_size==0)) {
      fprintf(stderr,"ERROR: Incorrect format: 5 elements while expecting 4 with (use_processing_size=0)\n");
      exit(1);
    }
    if ((r<4) && (r>5)) {
      fprintf(stderr,"ERROR while scanning input tree file\n");
      exit(1);
    }
    if (use_processing_size==-1) {
      switch (r) {
      case 4: use_processing_size = 0; break;
      case 5: use_processing_size = 1; break;
      default: break; 
      }
    }
    
    max_id = max_memdag(max_id, id);
    if (fgets(line, 255, input)==NULL) {
      break;
    }
  } 
  fclose(input);
 
  ///////////////////////// STEP 2: fill the tree
  input = fopen(filename, "r");

  if (!input) {
    fprintf(stderr,"Unable to open file %s\n", filename);
    abort();
  }
  int nb_of_nodes;
  tree_t tree;

  if (use_processing_size) {
    /// We create a tree with Liu model. Hence each task i is associated to to vertices: i_begin and i_end
    /// Here is the correspondance "in the tree structure" | "index in the file"
    /// 0 | 0 (root)
    /// 1 | 1_begin
    /// 2 | 1_end
    /// 3 | 2_begin
    /// 4 | 2_end
    /// ...
    /// 2N-1 | N_begin
    /// 2N | N_end
    
    
    nb_of_nodes = 2*max_id+1; 
    tree = (node_t*) calloc(nb_of_nodes, sizeof(node_t)); 
    assert(tree);
    
    /// Adding node 0: (not in the file)
    tree[0].id = 0;
    tree[0].name=strdup("root");
    tree[0].parent = NULL;
    tree[0].nb_of_children = 0;
    tree[0].max_nb_of_children = 0;
    tree[0].out_size = 0;
    tree[0].time = MIN_PROCESSING_TIME;
    
    
    do {
      fgets(line, 255, input);
    } while (line[0] == '%');
    while (sscanf(line,"%d %d %lld %lld %lld",&id, &parent, &processing_size, &out_size, &time) == 5) {
      assert(id < nb_of_nodes);
      assert(parent >= 0);
      char comment[255];
      int has_orig_comment = 0;
      if (sscanf(line,"%*d %*d %*d %*d %*d # %254s", comment)==1) {
	has_orig_comment=1;
      }
      
      DEBUG_TREES(fprintf(stderr,"Seen node %d with parent %d...",id,parent));
      
      /// Translation
      int orig_id = id;
      id = id*2-1;
      parent = max_memdag(2 * parent - 1, 0);
      
      DEBUG_TREES(fprintf(stderr,"allocated to node %d with parent %d with parent %d.\n",id,id+1,parent));
      char s[128];

      /// id_begin
      tree[id].id = id;
      snprintf(s,128,"begin_%d%s",orig_id, (has_orig_comment?comment:""));
      tree[id].name = strdup(s); 
      tree[id].parent = &(tree[id+1]);
      tree[id].nb_of_children = 0;
      tree[id].max_nb_of_children = 0;
      tree[id].children = NULL;
      tree[id].out_size = (double) (processing_size + out_size); // emulate our model (sum inputs + ni + outout) using Liu's model (max(sum inputs, outputs))
      // the sum of the inputs will be done when registering the children
      tree[id].time = max_memdag((double) time, MIN_PROCESSING_TIME);
      
      /// id_end
      tree[id+1].id = id+1;
      snprintf(s,128,"end_%d%s",orig_id, (has_orig_comment?comment:""));
      tree[id+1].name = strdup(s);
      tree[id+1].parent = &(tree[parent]);
      tree[id+1].nb_of_children = 0;
      tree[id+1].max_nb_of_children = 0;
      tree[id+1].children = NULL;
      tree[id+1].out_size = (double) out_size;
      tree[id+1].time = MIN_PROCESSING_TIME;
      
      if (fgets(line, 255, input)==NULL) {
	break;
      }
    }

    
    // Then register all nodes in their parent's children array, 
    // For end nodes only (with even index), add their output to the output of the father (input present during the computation of the father)
    for(id=0; id < nb_of_nodes; id++) {
      if (tree[id].parent) {
	//fprintf(stderr,"treating node %d with parent %d\n",id, tree[id].parent->id);
	if (tree[id].parent->nb_of_children == tree[id].parent->max_nb_of_children) {
	  tree[id].parent->max_nb_of_children = max_memdag(5, 2 * tree[id].parent->max_nb_of_children);
	  tree[id].parent->children = (node_t**) realloc(tree[id].parent->children, tree[id].parent->max_nb_of_children * sizeof(node_t *));
	}
	tree[id].parent->children[tree[id].parent->nb_of_children++] = &(tree[id]);
	if (id%2==0) {
	  tree[id].parent->out_size += tree[id].out_size;
	}
	//      fprintf(stderr,"Node %d registered at its parent %d\n",id, tree[id].parent->id);
      }
    }
  
  } else { // No processing size given, Liu's model
    nb_of_nodes = max_id+1; 
    tree = (node_t*) calloc(nb_of_nodes, sizeof(node_t)); 
    assert(tree);
    
    /// Adding node 0: (not in the file)
    tree[0].id = 0;
    tree[0].parent = NULL;
    tree[0].nb_of_children = 0;
    tree[0].max_nb_of_children = 0;
    tree[0].out_size = 0;
    tree[0].time = MIN_PROCESSING_TIME;
        
    do {
      fgets(line, 255, input);
    } while (line[0] == '%');
    
    while (sscanf(line,"%d %d %lld %lld",&id, &parent, &out_size, &time) == 4) {
      assert(id < nb_of_nodes);
      assert(parent >= 0);
      char comment[255];
      int has_orig_comment = 0;
      
      if (sscanf(line,"%*d %*d %*d %*d # %254s", comment)==1) {
	has_orig_comment=1;
	//fprintf(stderr, "Seen node with comment \"%s\"\n", comment);
      }

      DEBUG_TREES(fprintf(stderr,"Seen node %d with parent %d...",id,parent));
      
      tree[id].id = id;
      if (has_orig_comment) {
	tree[id].name = strdup(comment);
      }
      tree[id].parent = &(tree[parent]);
      tree[id].nb_of_children = 0;
      tree[id].max_nb_of_children = 0;
      tree[id].children = NULL;
      tree[id].out_size = (double) out_size;
      tree[id].time = max_memdag((double)time, MIN_PROCESSING_TIME);

      if (fgets(line, 255, input)==NULL) {
	break;
      }
    }

    // Then register all nodes in their parent's children array, 
    for(id=0; id < nb_of_nodes; id++) {
      if (tree[id].parent) {
	//fprintf(stderr,"treating node %d with parent %d\n",id, tree[id].parent->id);
	if (tree[id].parent->nb_of_children == tree[id].parent->max_nb_of_children) {
	  tree[id].parent->max_nb_of_children = max_memdag(5, 2 * tree[id].parent->max_nb_of_children);
	  tree[id].parent->children = (node_t**) realloc(tree[id].parent->children, tree[id].parent->max_nb_of_children * sizeof(node_t *));
	}
	tree[id].parent->children[tree[id].parent->nb_of_children++] = &(tree[id]);
      }
    }
  }
    
  fclose(input);
    
  
  update_subtree_stats(tree);
    
  DEBUG_TREES(fprintf(stderr,"created tree of size %d\n",tree->subtree_size));
  return tree;
}


void print_tree_as_node_list(FILE *out, tree_t tree) {
  int i;
  /* for(i=0; i<tree->subtree_size; i++) { */
  /*   fprintf(out,"%d %d %lld %lld # %s\n", i+1, ((i==0) ? 0 : (tree[i].parent->id)+1), (long long) tree[i].out_size,  (long long) tree[i].time, (tree[i].name?tree[i].name:"")); */
  /* } */
  for(i=1; i<tree->subtree_size; i++) {
    fprintf(out,"%d %d %lld %lld # %s\n", i, tree[i].parent->id, (long long) tree[i].out_size,  (long long) tree[i].time, (tree[i].name?tree[i].name:""));
  }
}

///@private
double compute_max_processing_time(tree_t tree) {
  double mpt = tree->time;
  int k;
  for(k=0; k<tree->nb_of_children; k++) {
    double local_mpt = compute_max_processing_time(tree->children[k]);
    mpt = max_memdag(mpt, local_mpt);
  }
  return mpt;
}

///@private
double compute_max_memory(tree_t tree) {
  double memory = max_memdag(tree->total_input_size, tree->out_size);
  int k;
  for(k=0; k<tree->nb_of_children; k++) {
    double local_memory = compute_max_memory(tree->children[k]);
    memory = max_memdag(memory, local_memory);
  }
  return memory;
}


/* void print_dot_tree(FILE *out, tree_t tree) { */
/*   int i; */
/*   double max_time = compute_max_processing_time(tree); */
/*   double max_memory = compute_max_memory(tree); */
/*   fprintf(out,"digraph G {\n"); */
/*   for(i=0; i<tree->subtree_size; i++) { */
/*     if ((i%2==1) || (i==0))  { */
/*       fprintf(out,"%d [shape=circle, label=\"%d (%s)\", width=1, fixedsize=true, style=filled,fillcolor=\"#8EC13A\"]\n", i, i,(tree[i].name?tree[i].name:"")); */
/*       //      fprintf(out,"%d [shape=rect, label=\"%d\", height=%f, width=%f, fixedsize=true, style=filled,fillcolor=\"#8EC13A\"]\n", i, i, tree[i].time / max_time * 20.0, max(tree[i].total_input_size, tree[i].out_size)/max_memory  * 20.0); */
/*       if (tree[i].parent) { */
/* 	fprintf(out,"%d -> %d [dir=back]\n", tree[i].parent->parent->id, i); */
/*       } */
/*     } */
/*   } */
/*   fprintf(out,"}\n"); */
/* } */

void print_dot_tree(FILE *out, tree_t tree) {
  int i;
  double max_time = compute_max_processing_time(tree);
  double max_memory = compute_max_memory(tree);
  fprintf(out,"digraph G {\n");
  for(i=0; i<tree->subtree_size; i++) {
    fprintf(out,"%d [shape=circle, label=\"%d (%s)\\n%lld\", width=1, fixedsize=true, style=filled,fillcolor=\"#8EC13A\"]\n", i, i,(tree[i].name?tree[i].name:""),
	    (long long) tree[i].time);

    if (tree[i].parent) {
      fprintf(out,"%d -> %d [dir=back, label=\"%lld\"]\n", tree[i].parent->id, i, (long long) tree[i].out_size);
    }
  }
  fprintf(out,"}\n");
}


tree_t copy_tree(tree_t tree) {
  tree_t new_tree;
  int new_leaf_counter = tree->subtree_size;
  
  new_tree = (node_t*) calloc(tree->subtree_size, sizeof(node_t));
    
  int i;
  for(i=0; i<tree->subtree_size; i++) {
    //    fprintf(stderr,"copying node %d\n",i);
    new_tree[i].id = tree[i].id;
    if (tree[i].name) { new_tree[i].name = strdup(tree[i].name); }
    new_tree[i].out_size = tree[i].out_size;
    new_tree[i].time = tree[i].time;
    new_tree[i].nb_of_children = tree[i].nb_of_children;
    new_tree[i].max_nb_of_children = new_tree[i].nb_of_children;
    
    if (tree[i].parent) {
      new_tree[i].parent = &(new_tree[tree[i].parent->id]); // careful here
    } else {
      new_tree[i].parent = NULL;
    }
    
    if (new_tree[i].nb_of_children > 0) {
      new_tree[i].children = (tree_t*)calloc(new_tree[i].nb_of_children, sizeof(tree_t));
    } else {
      new_tree[i].children = NULL;
    }
    
    int k;
    
    for(k=0; k<tree[i].nb_of_children; k++) { 
      new_tree[i].children[k] = &(new_tree[tree[i].children[k]->id]); // here too
    }
  }
  update_subtree_stats(new_tree);
  return new_tree;
}

void free_tree(tree_t tree) {
  int i;
  for(i=0; i<tree->subtree_size; i++) 
    free(tree[i].children);
  free(tree);
}

/**
 * Checks if a given directed acyclic graph is a tree
 * All vertices in the graph should have an out-degree of 1, except
 * the target (out-degree 0). If several targets exist and \p
 * add_common_root_if_forest is set to one, a target is added to the
 * graph as well as corresponding edges from out-degree 0 nodes to
 * this target. In case of success, set the "target" field to
 */


int check_if_tree(graph_t *graph, int add_common_root_if_forest) {
  vertex_t *target = NULL;
  int several_targets = 0;
  
  for(vertex_t *v = graph->first_vertex; v; v=v->next) {
    if (v->out_degree > 1) {
      //      fprintf(stderr,"out_degree > 1\n");
      return 0;
    }
    if (v->out_degree==0) {
      if (target == NULL) { // If no current target, update
	target = v;
	//	fprintf(stderr,"found 1 target\n");
      } else { // several targets
	target = NULL;
	several_targets = 1;
	//	fprintf(stderr,"found 1 target\n");
      }
    }
  }

  if (several_targets && add_common_root_if_forest) {
    target = new_vertex(graph, "GRAPH_TARGET", 0.0, NULL);
    for(vertex_t *v = graph->first_vertex; v; v=v->next) {
      if ((v->out_degree==0) && (v!= target)) {
	new_edge(graph, v, target, 0, NULL);
      }
    }
  }

  if ((!several_targets) || add_common_root_if_forest) {
    graph->target = target;
    return 1;
  } else {
    return 0;
  }
}

/**
 * Convert from graph format to tree format
 * All vertices in the graph should have an out-degree of 1, except the target (out-degree 0). Otherwise, NULL is returned. 
 */

tree_t graph_to_tree(graph_t *graph) {
  if (check_if_tree(graph,0)==0)
    return NULL;

  tree_t tree = (node_t*) calloc(graph->number_of_vertices, sizeof(node_t));

  /* First pass: register IDs of tree nodes (target=root should be at id 0) */
  int *graph_id_to_tree_id =calloc(graph->next_vertex_index, sizeof(int));
  int next_tree_id=1;
  for(vertex_t *v = graph->first_vertex; v; v=v->next) {
    if (v != graph->target) {
      graph_id_to_tree_id[v->id] = next_tree_id;
      next_tree_id++;
    } else {
      graph_id_to_tree_id[v->id] = 0;
    }
  }

  /* Second pass: fill children/parent fields */
  for(vertex_t *v = graph->first_vertex; v; v=v->next) {
    int id = graph_id_to_tree_id[v->id];
    tree[id].id = id;
    if (v->out_degree > 0) {
      tree[id].out_size = v->out_edges[0]->weight;
      tree[id].parent = &tree[graph_id_to_tree_id[v->out_edges[0]->head->id]];
    }
    tree[id].time = v->time;
    tree[id].nb_of_children = v->in_degree;
    tree[id].max_nb_of_children = v->in_degree;
    tree[id].children = (node_t**) calloc(v->in_degree, sizeof(node_t*));
    for(int i=0; i<v->in_degree; i++) {
      tree[id].children[i] = &tree[graph_id_to_tree_id[v->in_edges[i]->tail->id]];
    }
  }
  free(graph_id_to_tree_id);
  update_subtree_stats(tree);
  return tree;  
}


graph_t *tree_to_graph(tree_t tree) {
  graph_t *graph = new_graph();

  vertex_t **graph_vertex_by_tree_id = (vertex_t**) calloc(tree->subtree_size, sizeof(vertex_t*));

  for(int i=0;i<tree->subtree_size; i++) {
    char s[128];
    char *name;
    if (tree[i].name) {
      name = tree[i].name;
    } else {
      snprintf(s,128,"node_%d",i);
      name = s;
    }
    graph_vertex_by_tree_id[i] = new_vertex(graph, name, tree[i].time, NULL);
  }
  for(int i=0;i<tree->subtree_size; i++) {
    if (tree[i].parent) {
      new_edge(graph,graph_vertex_by_tree_id[i], graph_vertex_by_tree_id[tree[i].parent->id], tree[i].out_size, NULL);
    }
  }
  
  free(graph_vertex_by_tree_id);
  return graph;
}


