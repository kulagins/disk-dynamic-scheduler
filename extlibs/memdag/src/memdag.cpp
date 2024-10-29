#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include "memdag.hpp"

int runMemdag(int argc, char **argv) {

  int to_sp_graph = 0;
  int check_sp = 0;
  int min_mem = 0;
  int max_mem = 0;
  int is_tree = 0;
  int input_is_list_of_nodes = 0;
  int postorder = 0;
  char *sp_filename = NULL;
  int c;
  int remove_one = 0;
  char *memory_label = "weight";
  char *timing_label = NULL;
  char *node_memory_label = NULL;
  int restrict_memory = 0;
  double memory_bound = -1.0;
  edge_selection_heuristic_t restrict_heur=MIN_LEVEL;
  int show_help = 0;
  
  while ((c = getopt (argc, argv, "cs:mMtTlpw:n:r:h:")) != -1)
    switch (c) {
    case 's':
      to_sp_graph = 1;
      sp_filename = optarg;
      break;
    case 'c':
      check_sp = 1;
      break;
    case 'm':
      min_mem = 1;
      break;
    case 'M':
      max_mem = 1;
      break;
    case 'T':
      is_tree = 1;
      break;
    case 'l':
      is_tree = 1;
      input_is_list_of_nodes = 1;
      break;
    case 'p':
      postorder = 1;
      break;
    case 'w':
      memory_label = optarg;
      break;
    case 't':
      timing_label = optarg;
      break;
    case 'n':
      node_memory_label = optarg;
      break;
    case 'r':
      restrict_memory = 1;
      memory_bound = atof(optarg);
      break;
    case 'h':
	restrict_memory = 1;
	if (strcmp(optarg,"respectorder")==0) {
	  restrict_heur = RESPECT_ORDER;
	} else if (strcmp(optarg,"minlevel")==0) {
	  restrict_heur = MIN_LEVEL;
	} else if (strcmp(optarg,"maxsize")==0) {
	  restrict_heur = MAX_SIZE;
	} else if (strcmp(optarg,"maxminsize")==0) {
	  restrict_heur = MAX_MIN_SIZE;
	} else {
	  fprintf(stderr,"Unknown restrict_memory heuristic \"%s\"\n", optarg);
	  exit(1);
	}
	break;
      case '?':
        if (optopt == 's') {
	  fprintf (stderr, "Missing file name with option -s\n");
	} else if (optopt == 'w' || optopt == 'n') {
	  fprintf (stderr, "Missing argument with option -w or -n\n");
	} else if (isprint (optopt)) {
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        } else {
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
	}
	show_help = 1;
	break;
      default:
        abort ();
      }

  
  if (argc != optind+1 || show_help==1) {
    //fprintf(stderr,"argc:%d optind:%d\n", argc, optind);
    fprintf(stderr,"Usage %s [options] <graph.dot>\n", argv[0]);
    fprintf(stderr,"options:\t -s [<sp_graph_filename>] : compute SP-graph from general graph\n");
    fprintf(stderr,"        \t -m : compute minimal memory peak and corresponding schedule (of corresponding sp-graph if general graph)\n");
    fprintf(stderr,"        \t -c : check if the given graph is SP\n");
    fprintf(stderr,"        \t -M : compute maximal parallel memory peak\n");
    fprintf(stderr,"        \t -T : the input graph is a tree, use specialized optimizations\n");
    fprintf(stderr,"        \t -l : the input graph is a tree, provided as a list of nodes\n");
    fprintf(stderr,"        \t -w <memory_label> : set the memory label of the edges when reading the dot file (default:\"weight\")\n");
    fprintf(stderr,"        \t -t <timing_label> : set the timing label of the nodes when reading the dot file (default:none, all nodes have weight 1)\n");
    fprintf(stderr,"        \t -n <node_memory_label> : set the node memory label, switch to IPDPS'11 model (otherwise, IPDPS'18 model used)\n");
    fprintf(stderr,"        \t -r <memory_bound> : add edges to limit the size of the maximal peak memory\n");
    fprintf(stderr,"        \t -h minlevel|respectorder|maxsize|maxminsize : set the heuristic for selecting edges when limiting the size of the maximal peak memory (default:minlevel)\n");
    return 1;
  }

  //fprintf(stderr,"argc:%d optind:%d %s\n", argc, optind, argv[optind]);
  char *input_graph_filename = argv[optind];
    
  
  if (to_sp_graph == 0 &&  check_sp == 0 && postorder==0 && min_mem==0 && max_mem==0 && restrict_memory==0) {
    fprintf(stderr,"Error: nothing to do!\n");
    exit(1);
  }

 

  graph_t * graph;
  tree_t tree;
  int graph_is_a_tree=0;
  if (input_is_list_of_nodes) {
    tree = read_tree_as_node_list(input_graph_filename);
    graph_is_a_tree = 1;
  } else {
    graph = read_dot_graph(input_graph_filename, memory_label, timing_label, node_memory_label);
    graph_is_a_tree = check_if_tree(graph,0);    
  }
  
  if (is_tree && graph_is_a_tree==0) {
    fprintf(stderr,"Option -T used but provided graph is not an in-tree, aborting.\n");
    exit(1);
  }
  
  if (min_mem==1 && graph_is_a_tree==0) { // In this case, sp_graph is needed to compute the minimum memory (heuristic)
    fprintf(stdout,"Warning: minimum memory will be computed through sp-ization, which only gives an upper bound!\n");
    to_sp_graph = 1;
  }

  if (min_mem==1 && graph_is_a_tree==1) { // to handle out-trees
    to_sp_graph = 1;
  }
  
  if (input_is_list_of_nodes && (max_mem || restrict_memory)) {
    graph = tree_to_graph(tree);
    free_tree(tree);
    is_tree=0;
  }
  if (is_tree) {
    if (!input_is_list_of_nodes) {
      tree = graph_to_tree(graph);
    }
    int *schedule;
    if (min_mem) {
      fprintf(stdout,"Optimal sequantial memory: %f\n", Liu_optimal_sequential_memory(tree, &schedule));
      for(int i=0;i<tree->subtree_size; i++) {
	fprintf(stdout,"%d%s%s%s%s",
		schedule[i],
		(tree[schedule[i]].name?" (":""),		
		(tree[schedule[i]].name?tree[schedule[i]].name:""),
		(tree[schedule[i]].name?")":""),		
		((i==tree->subtree_size - 1)?"\n":", "));
      }
    }
    if (postorder) {
      fprintf(stdout,"Best postorder sequantial memory: %f\n", best_postorder_sequential_memory(tree, &schedule));
      for(int i=0;i<tree->subtree_size; i++) {
	fprintf(stdout,"%d%s%s%s%s",
		schedule[i],
		(tree[schedule[i]].name?" (":""),		
		(tree[schedule[i]].name?tree[schedule[i]].name:""),
		(tree[schedule[i]].name?")":""),		
		((i==tree->subtree_size - 1)?"\n":", "));
      }
      free(schedule);
    }
  } else { // graph is not a tree
    
    SP_tree_t *sp_tree= NULL;
    graph_t *sp_graph = NULL;
    
    if (to_sp_graph==1 || check_sp==1) {
      enforce_single_source_and_target(graph);
      sp_tree = build_SP_decomposition_tree(graph);
      
      
      if (check_sp==1) {
	if (sp_tree) {
	  fprintf(stdout,"This is a SP graph.\n");
	} else {
	  fprintf(stdout,"This is not a SP graph.\n");
	}
      }
      
      if (sp_tree) {
	sp_graph = graph;
      } else {
	sp_graph = graph_sp_ization(graph);
	sp_tree = build_SP_decomposition_tree(sp_graph);
      }
    }
    
    
    if (sp_filename) {
      FILE *output_sp_file = fopen(sp_filename,"w");
      if (output_sp_file==NULL) {
	fprintf(stderr,"Unable to write in file \"%s\"\n",sp_filename);
	exit(1);
      }
      print_graph_to_dot_file(sp_graph, output_sp_file);
      compute_bottom_and_top_levels(graph);
      compute_bottom_and_top_levels(sp_graph);
      fprintf(stdout,"Critical path of original graph: %e\n", graph->source->bottom_level);
      fprintf(stdout,"Critical path of sp-ized graph: %e\n", sp_graph->source->bottom_level);
      fprintf(stdout,"Increase ratio: %e\n", sp_graph->source->bottom_level /graph->source->bottom_level);
      fclose(output_sp_file);
    }
    
    if (min_mem==1) {
      assert(sp_graph);
      assert(sp_tree);
      vertex_t **schedule = compute_optimal_SP_traversal(sp_graph, sp_tree);
      fprintf(stdout,"optimal schedule:\n");
      for(int i=0; i<sp_graph->number_of_vertices; i++)
	fprintf(stdout,"%s%s",(i==0)?"":", ", schedule[i]->name.c_str());
      fprintf(stdout,"\n");
      fprintf(stdout,"Peak memory: %f\n", compute_peak_memory(sp_graph, schedule));
      if (graph != sp_graph)
	free_graph(sp_graph); 
    }
    
    if (max_mem==1) {
      double maxmem = maximum_parallel_memory(graph);
      fprintf(stdout,"Max. para. memory: %f\n", maxmem);
    }
    if (restrict_memory==1) {
      add_edges_to_cope_with_limited_memory(graph, memory_bound, restrict_heur);
      //remove_transitivity_edges_weight_conservative(graph);
      print_graph_to_dot_file(graph,stdout);
      fprintf(stderr,"New edges:\n");
      for(edge_t *e=graph->first_edge; e; e=e->next) {
	if (e->status == ADDED) {
	  fprintf(stderr,"%s -> %s\n",e->tail->name.c_str(),e->head->name.c_str());
	}
      }
    }
    
    free(graph);
  }
  return 0;
}
