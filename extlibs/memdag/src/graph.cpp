#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <graphviz/cgraph.h>
#include <iostream>
#include <algorithm>
#include "fifo.hpp"
#include "graph.hpp"

/** \file graph.c
 * \brief Graph management for memdag
 */

//#define DEBUG_GRAPH
/**
 * Creates a new empty graph
 */

graph_t *new_graph(void) {
  return (graph_t*) calloc(1, sizeof(graph_t));
}

///\cond HIDDEN_SYMBOLS
vertex_t *new_vertex_with_id(graph_t *graph, int id, const std::string name, double time, void *data) {
  vertex_t *new_vertex = (vertex_t*) calloc(1,sizeof(vertex_t));
  new_vertex->name = name;
  new_vertex->data = data;
  new_vertex->time = time;
  if (id<0) {
    new_vertex->id = graph->next_vertex_index;
    graph->next_vertex_index++;
  } else {
    new_vertex->id = id;
    graph->next_vertex_index = max_memdag(graph->next_vertex_index, id + 1);
  }
  
  vertex_t *old_first = graph->first_vertex;
  new_vertex->next = old_first;
  new_vertex->prev = NULL;
  if (old_first) {
    old_first->prev = new_vertex;
  }
  graph->first_vertex = new_vertex;

  if (graph->next_vertex_index >= graph->vertices_by_id_size) {
    graph->vertices_by_id_size = max_memdag(graph->vertices_by_id_size * 2, graph->next_vertex_index + 7);
    graph->vertices_by_id = (vertex_t**) realloc(graph->vertices_by_id, graph->vertices_by_id_size * sizeof(vertex_t*));
  }
  graph->vertices_by_id[new_vertex->id] = new_vertex;

  graph->number_of_vertices++;
  
  return new_vertex;
}
///\endcond

/**
 * Creates a new vertex in an existing graph
 * @param graph the graph where to add the vertex
 * @param name vertex' name
 * @param time vertex duration
 * @param data pointer to user data (possibly void)
 *
 * Note that the name of the vertex is duplicated, so that \p name can be safely freed.
 */


vertex_t *new_vertex(graph_t *graph, const std::string name, double time, void *data) {
  vertex_t *new_vertex = new_vertex_with_id(graph, -1, name, time, data);
  return new_vertex;
}

vertex_t *new_vertex2Weights(graph_t *graph, const char *name, double time, double memRequirement, void *data) {
    vertex_t *new_vertex = new_vertex_with_id(graph, -1, name, time, data);
    new_vertex->memoryRequirement = memRequirement;
    return new_vertex;
}
/**
 * Creates a new edge in an existing graph
 * @param graph the graph where to add the edge
 * @param tail edge's origin
 * @param head edge's target
 * @param weight memory weight of the edge
 * @param data pointer to user data (possibly void)
 */

edge_t *new_edge(graph_t *graph, vertex_t *tail, vertex_t *head, double weight, void *data) {
  edge_t *new_edge = (edge_t*) calloc(1,sizeof(edge_t));
  new_edge->weight = weight;
  new_edge->data = data;    
  new_edge->tail = tail;
  new_edge->head = head;

  // register edge at tail vertex
  if (tail->out_size == 0) {
    tail->out_size = 4;
    tail->out_edges = (edge_t**) calloc(tail->out_size, sizeof(edge_t*));
  }
  if (tail->out_degree == tail->out_size) {
    tail->out_size *= 2;
    tail->out_edges = (edge_t**) realloc(tail->out_edges, tail->out_size * sizeof(edge_t*));
  }
  tail->out_edges[tail->out_degree] = new_edge;
  tail->out_degree++;

  // register edge at head vertex
  if (head->in_size == 0) {
    head->in_size = 4;
    head->in_edges = (edge_t**)calloc(head->in_size, sizeof(edge_t*));
  }
  if (head->in_degree == head->in_size) {
    head->in_size *= 2;
    head->in_edges = (edge_t**) realloc(head->in_edges, head->in_size * sizeof(edge_t*));
  }
  head->in_edges[head->in_degree] = new_edge;
  head->in_degree++;
  
  edge_t *old_first = graph->first_edge;
  new_edge->next = old_first;
  new_edge->prev = NULL;
  if (old_first) {
    old_first->prev = new_edge;
  }
  graph->first_edge = new_edge;

  graph->number_of_edges++;
  
  return new_edge;

}

/**
 * Remove an existing vertex in a graph
 */

void remove_vertex(graph_t *graph, vertex_t *v) {
  if (graph->vertices_by_id[v->id] != v) {
    fprintf(stderr,"ERROR: attempt to remove vertex \"%s\" from another graph.\n",v->name.c_str());
    exit(1);
  }
  fifo_t *edges_to_be_suppressed = fifo_new();
  // Remove edges associated to vertex
  for(int i=0; i<v->in_degree; i++) {
    fifo_write(edges_to_be_suppressed, (void*)v->in_edges[i]);
  }
  for(int i=0; i<v->out_degree; i++) {
    fifo_write(edges_to_be_suppressed, (void*)v->out_edges[i]);
  }

  while (!fifo_is_empty(edges_to_be_suppressed)) {
    edge_t* e = (edge_t*) fifo_read(edges_to_be_suppressed);
    remove_edge(graph, e);
  }
  fifo_free(edges_to_be_suppressed);

  
  // Remove from vertex list and array
  vertex_t *n = v->next;
  vertex_t *p = v->prev;
  if (n)
    n->prev = p;
  if (p) {
    p->next = n;
  } else {
    graph->first_vertex = n;
  }
  graph->vertices_by_id[v->id] = NULL;

  graph->number_of_vertices--;
  
  // Free vertex data
  free(v->in_edges);
  free(v->out_edges);
  if(v->subgraph!=NULL)
    free_graph(v->subgraph);
  //free(v->name);
  free(v);
}


/**
 * Remove an existing edge in a graph
 */


void remove_edge(graph_t *graph, edge_t *e) {
  vertex_t *tail = e->tail;
  vertex_t *head = e->head;

  if (graph->vertices_by_id[tail->id] != tail) {
    fprintf(stderr,"ERROR: attempt to remove edge \"%s->%s\" from another graph.\n", tail->name, head->name);
    exit(1);
  }

  //fprintf(stderr,"removing edge  \"%s->%s\" \n", tail->name, head->name);
  
  // Remove in tail out_edges
  int index_in_tail_out_edges=0;
  for(int i=0; i<tail->out_degree; i++) {
    if (tail->out_edges[i]==e) {
      index_in_tail_out_edges = i;
      break;
    }
  }
  assert(index_in_tail_out_edges<tail->out_degree);
  for(int i=index_in_tail_out_edges+1; i<tail->out_degree; i++) {
    tail->out_edges[i-1] = tail->out_edges[i];
  }
  tail->out_degree--;
  tail->out_edges[tail->out_degree] = NULL;

  // Remove in head in_edges
  int index_in_head_in_edges=0;
  for(int i=0; i<head->in_degree; i++) {
    if (head->in_edges[i]==e) {
      index_in_head_in_edges = i;
      break;
    }
  }
  assert(index_in_head_in_edges<head->in_degree);
  for(int i=index_in_head_in_edges+1; i<head->in_degree; i++) {
    head->in_edges[i-1] = head->in_edges[i];
  }
  head->in_degree--;
  head->in_edges[head->in_degree] = NULL;

  // Remove from edge list
  edge_t *n = e->next;
  edge_t *p = e->prev;
  if (n)
    n->prev = p;
  if (p) {
    p->next = n;
  } else {
    graph->first_edge = n;
  }

  graph->number_of_edges--;

  // Free edge
  free(e);
}

/**
 * Prints a graph as a dot file
 */


void print_graph_to_dot_file(graph_t *graph, FILE *output) {
  fprintf(output,"DiGraph G {\n");
  //  for(vertex_t *v=first_vertex(graph); is_last_vertex(v); v=next_vertex(v)) {
  for(vertex_t *v=graph->first_vertex; v; v=v->next) {
    fprintf(output," %d [label=\"%s (%d) - %f\"];\n", v->id, v->name.c_str(), v->id, v->time);
  }
  for(edge_t *e=graph->first_edge; e; e=e->next) {
    fprintf(output," %d -> %d [label=\"%f\" path=\"%s -> %s\"%s];\n", e->tail->id, e->head->id, e->weight, e->tail->name.c_str(), e->head->name.c_str(), (e->status==IN_CUT)?" color=\"red\"":(e->status==ADDED)?" color=\"blue\"":"");
  }
  fprintf(output,"}\n");
}

void print_graph_to_cout(graph_t *graph) {
    printf("DiGraph G {\n");
    //  for(vertex_t *v=first_vertex(graph); is_last_vertex(v); v=next_vertex(v)) {
    for(vertex_t *v=graph->first_vertex; v; v=v->next) {
        printf(" %d [label=%s (%d), leader=%d time= %f memory= %f];\n", v->id, v->name.c_str(), v->id, v->leader, v->time, v->memoryRequirement);
    }
    for(edge_t *e=graph->first_edge; e; e=e->next) {
        printf(" %d -> %d [weight=\"%f\" path=\"%s -> %s\"%s];\n", e->tail->id, e->head->id, e->weight, e->tail->name.c_str(), e->head->name.c_str(), (e->status==IN_CUT)?" color=\"red\"":(e->status==ADDED)?" color=\"blue\"":"");
    }
    printf("}\n");
}

void print_graph_to_cout_full(graph_t *graph) {
    printf("DiGraph G, full {\n");
    //  for(vertex_t *v=first_vertex(graph); is_last_vertex(v); v=next_vertex(v)) {
    for(vertex_t *v=graph->first_vertex; v; v=v->next) {
        printf(" %d, leader %d [label=%s (%d) time= %f memory= %f];\n", v->id, v->leader, v->name.c_str(), v->id, v->time, v->memoryRequirement);
        if(v->subgraph!=NULL){
            for(vertex_t *v1=v->subgraph->first_vertex; v1; v1=v1->next) {
                printf("\t %d [label=%s (%d) leader= %d time= %f memory= %f];\n", v1->id, v1->name.c_str(), v1->id, v->leader, v1->time, v1->memoryRequirement);
            }
        }

    }

    for(edge_t *e=graph->first_edge; e; e=e->next) {
        printf(" %d -> %d [weight=\"%f\" path=\"%s -> %s\"%s];\n", e->tail->id, e->head->id, e->weight, e->tail->name.c_str(), e->head->name.c_str(), (e->status==IN_CUT)?" color=\"red\"":(e->status==ADDED)?" color=\"blue\"":"");
    }
    printf("}\n");
}

/**
 * Creates a copy of an existing graph
 */


graph_t *copy_graph(graph_t *graph, int reverse_edges) {
  graph_t *new_g = new_graph();
  new_g->vertices_by_id = (vertex_t**) calloc(graph->vertices_by_id_size+1, sizeof(vertex_t*)); // TODO: not really needed as it is reallocated as needed
  for(vertex_t *v=graph->first_vertex; v; v=v->next) {
    new_vertex_with_id(new_g, v->id, v->name, v->time, v->data);
    new_g->vertices_by_id[v->id]->memoryRequirement = v->memoryRequirement;
    new_g->vertices_by_id[v->id]->leader = v->leader;
    new_g->vertices_by_id[v->id]->makespan = v->makespan;
    if(v->subgraph!=NULL)
        new_g->vertices_by_id[v->id]->subgraph = copy_graph(v->subgraph,0);
    new_g->vertices_by_id[v->id]->assignedProcessor = v->assignedProcessor;
  }
  
  for(edge_t *e=graph->first_edge; e; e=e->next) {
#ifdef DEBUG_GRAPH
    fprintf(stderr,"Check original edge %s->%s\n", e->tail->name, e->head->name);
    find_edge(e->tail, e->head);
#endif
    if (reverse_edges == 0) {
      new_edge(new_g, new_g->vertices_by_id[e->tail->id], new_g->vertices_by_id[e->head->id], e->weight, e->data);
#ifdef DEBUG_GRAPH
      fprintf(stderr,"Check newly created edge %s->%s\n",new_g->vertices_by_id[e->tail->id]->name,new_g->vertices_by_id[e->head->id]->name);
      find_edge(new_g->vertices_by_id[e->tail->id], new_g->vertices_by_id[e->head->id]);
#endif
    } else {
      new_edge(new_g, new_g->vertices_by_id[e->head->id], new_g->vertices_by_id[e->tail->id], e->weight, e->data);
#ifdef DEBUG_GRAPH
      fprintf(stderr,"Check newly created (reversed) edge %s->%s\n",new_g->vertices_by_id[e->head->id]->name,new_g->vertices_by_id[e->tail->id]->name);
      find_edge(new_g->vertices_by_id[e->head->id], new_g->vertices_by_id[e->tail->id]);
#endif
    }
  }
  new_g->number_of_vertices = graph->number_of_vertices;
  new_g->number_of_edges = graph->number_of_edges;
  if (graph->source) {
    if (reverse_edges == 0){
      new_g->source = new_g->vertices_by_id[graph->source->id];
    } else {
      new_g->target = new_g->vertices_by_id[graph->source->id];
    }
  } 
  if (graph->target) {
    if (reverse_edges == 0) {
      new_g->target = new_g->vertices_by_id[graph->target->id];
    } else {
      new_g->source = new_g->vertices_by_id[graph->target->id];
    }
  }
  return new_g;
}

/**
 * Converts a graph to igraph format
 * @param graph the graph to export
 * @param edge_weights_p pointer to a vector for storing edge weights
 * @param node_names_p pointer to a string vector for storing node names
 * @param node_times_p pointer to vector for storing node durations
 *
 * The id of the nodes are preserved by this export procedure. Each of
 * the three vector parameters may be NULL. If not NULL, the vector
 * will first be initialized and then filled with the corresponding
 * data.
 */

igraph_t convert_to_igraph(graph_t *graph, igraph_vector_t *edge_weights_p, igraph_strvector_t *node_names_p, igraph_vector_t *node_times_p) {
  
  if (node_names_p) {
    igraph_strvector_init(node_names_p, graph->next_vertex_index);
    for(vertex_t *v=graph->first_vertex; v; v=v->next) {
      igraph_strvector_set(node_names_p, v->id, v->name.c_str());
    }
  }

  if (node_times_p) {
    igraph_vector_init(node_times_p, graph->next_vertex_index);
    for(vertex_t *v=graph->first_vertex; v; v=v->next) {
      igraph_vector_set(node_times_p, v->id, v->time);
    }
  }

  igraph_vector_t edges;
  igraph_vector_init(&edges,0);
  if (edge_weights_p) {
    igraph_vector_init(edge_weights_p,0);
  }

  for(edge_t *e=graph->first_edge; e; e=e->next) {
    igraph_vector_push_back(&edges, e->tail->id);
    igraph_vector_push_back(&edges, e->head->id);
    if (edge_weights_p) {
      igraph_vector_push_back(edge_weights_p, e->weight);
    }
  }
  igraph_t igraph;
  igraph_create(&igraph, &edges, 0, IGRAPH_DIRECTED);
  igraph_vector_destroy(&edges);

 return igraph;
}

/**
 * Free a graph
 *
 * Note: user data associated to nodes and/or edges is kept untouched.
 */
void free_graph(graph_t *graph) {
  vertex_t *v = graph->first_vertex;
  if(v){
      do {
          vertex_t *vv = v;
          v=vv->next;
          //free(vv->name);
          free(vv->in_edges);
          free(vv->out_edges);
          free(vv);
      } while (v);
  }

  
  edge_t *e = graph->first_edge;
  if(e) {
      do {
          edge_t *ee = e;
          e = ee->next;
          free(ee);
      } while (e);
  }

  free(graph->vertices_by_id);
  free(graph);
}

/**
 * Read graphs in dot format from file.
 * @param filename name of the file containing the graph
 * @param memory_label optional label of edges' memory
 * @param timing_label optional label for nodes' processing time
 * @param node_memory_label optional label for nodes' memory need
 *
 * If \p memory_label is omitted (NULL), then all edges are assumed to
 * have weight 1. Similarly, when \p timing_label is set to NULL, then
 * all nodes have duration 1.
 *
 * If \p node_memory_label is set to NULL, then only edges have memory
 * weight, and when a node's processing is started, its input are
 * immediately freed and its output are allocated (memory model of
 * paper IPDPS'18). Otherwise, during the processing of a node, the
 * memory include its inputs, its outputs and the node memory need
 * (defined by \p node_memory_label).
 */

graph_t *read_dot_graph(const char *filename, const char *memory_label, const char *timing_label, const char *node_memory_label) {
  // If memory_label is NULL, all edges are assumed of
  // weight=1. Similarly, if timing_label is NULL, all durations are
  // set to 1.

  FILE* input_graph_file = fopen(filename,"r");
  if (!input_graph_file) {
    fprintf(stderr, "Unable to read file \"%s\"\n",filename);
    exit(1);
  }
  Agraph_t *ag_graph=agread(input_graph_file, 0);
  fclose(input_graph_file); 
  
  graph_t *graph = new_graph();
  graph->next_vertex_index=1;
  
  /*
   * 1. Assign id to nodes, going from 0 to n-1, 
   */
  
  typedef struct my_node_s {
    Agrec_t header;
    /* programmer-defined fields follow */
    vertex_t *in_vertex, *out_vertex;
  } my_node_vertex_t;
  aginit(ag_graph, AGNODE, "my_node_vertex_t", sizeof(my_node_vertex_t), TRUE);
  ///\cond HIDDEN_SYMBOLS
#define MY_NODE_IN_VERTEX(node) ((my_node_vertex_t *) (node->base.data))->in_vertex
#define MY_NODE_OUT_VERTEX(node) ((my_node_vertex_t *) (node->base.data))->out_vertex
  ///\endcond

  for(Agnode_t *ag_node = agfstnode(ag_graph); ag_node; ag_node = agnxtnode(ag_graph,ag_node)) {
    double time;
    double node_memory;
    char *name = agget(ag_node, (char *) "label");
    char *name2 = agnameof(ag_node);
    if (timing_label) {
      time = strtod(agget(ag_node, (char*) timing_label), NULL);
    } else {
      time = 1.0;
    }
      vertex_t *someV = new_vertex(graph, name, time, NULL);
      MY_NODE_IN_VERTEX(ag_node) = MY_NODE_OUT_VERTEX(ag_node) = someV;
    if (node_memory_label) {
      node_memory = strtod(agget(ag_node, (char*) node_memory_label), NULL);
    /*  char *in_name = (char*) calloc(strlen(agnameof(ag_node))+3,sizeof(char));
      char *out_name = (char*) calloc(strlen(agnameof(ag_node))+3,sizeof(char));
      snprintf(in_name, strlen(agnameof(ag_node))+4, "%s_in",agnameof(ag_node));
      snprintf(out_name, strlen(agnameof(ag_node))+5, "%s_out",agnameof(ag_node));
      MY_NODE_IN_VERTEX(ag_node) = new_vertex(graph, in_name, time, NULL);
      MY_NODE_OUT_VERTEX(ag_node) = new_vertex(graph, out_name, 0.0, NULL);
      free(in_name);
      free(out_name); 
      edge_t *e = new_edge(graph, MY_NODE_IN_VERTEX(ag_node), MY_NODE_OUT_VERTEX(ag_node), node_memory, NULL);*/
      someV->memoryRequirement = node_memory;
    } else {
      
    }

  }
 
  /* 
   * 2.Get the edges and their memory 
   */
  for (Agnode_t *ag_node = agfstnode(ag_graph); ag_node; ag_node = agnxtnode(ag_graph,ag_node)) {
    vertex_t *tail = MY_NODE_OUT_VERTEX(ag_node);
    for (Agedge_t *ag_edge = agfstout(ag_graph,ag_node); ag_edge; ag_edge = agnxtout(ag_graph,ag_edge)) {
      vertex_t *head = MY_NODE_IN_VERTEX(aghead(ag_edge));
      double edge_weight;
      if (memory_label) {
	char *str = agget(ag_edge, (char*) memory_label);
	if (!str) {
	  fprintf(stderr,"Unable to find memory attribue with label \"%s\" in graph %s\n",memory_label, filename);
	  exit(1);
	}
	edge_weight = strtod(str, NULL);
      } else {
	edge_weight = 1.0;
      }
      new_edge(graph, tail, head, edge_weight, NULL);
      /* 
       * In case of a graph with node memory weight, add this weight
       * to the processing of the source and target vertices. Note
       * that the source (resp. target) vertex has a single input
       * (resp. output) edge.
       */
   //   if (node_memory_label) {
//	tail->in_edges[0]->weight += edge_weight;
	//head->out_edges[0]->weight += edge_weight;
     // }
#ifdef DEBUG_GRAPH
      fprintf(stderr, "Check edge creation %s->%s\n",tail->name, head->name);
      find_edge(tail, head);
#endif
    }
  }


  agclose(ag_graph);
  return graph;
}

/**
 * Ensure that a graph a single source and target, and fills the corresponding fields of the graph data structure.
 *
 * If a graph a several sources (respectively targets), a new vertex
 * is created and edges are added from (resp. to) this vertex to
 * (resp. from) the existing sources (resp. targets).
 */
void enforce_single_source_and_target(graph_t *graph, std::string suffix) {
  vertex_t *source=NULL, *target=NULL;

  /*
   * Check if we find one or several source(s) and target(s)
   */
  int several_sources = 0;
  int several_targets = 0;
  for(vertex_t *v = graph->first_vertex; v; v=v->next) {
    if (v->in_degree==0) {
      if (source == NULL) { // If no current source, update
	source = v;
      } else {
	source = NULL;
	several_sources = 1;
      }
    }
    if (v->out_degree==0) {
      if (target == NULL) { // If no current target, update
	target = v;
      } else { // If several targets, remember it
	target = NULL;
	several_targets = 1;
      }
    }
  }


  /*
   * If several sources are detected, create new vertex and connect it to existing sources
   */
  if (several_sources) {
    source = new_vertex(graph, "GRAPH_SOURCE"+suffix, 0.0, NULL);
    for(vertex_t *v = graph->first_vertex; v; v=v->next) {
      if ((v->in_degree==0) && (v!= source)) {
	new_edge(graph, source, v, 0, NULL);
      }
    }
  }

  /*
   * If several targets are detected, create new vertex and connect existing targets to it
   */
  if (several_targets) {
    target = new_vertex(graph, "GRAPH_TARGET"+suffix, 0.0, NULL);
    for(vertex_t *v = graph->first_vertex; v; v=v->next) {
      if ((v->out_degree==0) && (v!= target)) {
	new_edge(graph, v, target, 0, NULL);
      }
    }
  }
  graph->source = source;
  graph->target = target;
}

/**
 * Returns the edge going from the tail vertex to the head vertex in the graph
 *
 * Returns NULL if the edge doesn't exists. If serveral edges go from
 * tail to head, any of these edges may be returned.
 */
edge_t *find_edge(vertex_t *tail, vertex_t *head) {
#ifdef DEBUG_GRAPH
  fprintf(stderr, "find_edge called with tail:%s (out_degree:%d) head:%s\n", tail->name, tail->out_degree, head->name);
#endif
  for(int i=0; i<tail->out_degree; i++) {
    edge_t *e = tail->out_edges[i];
#ifdef DEBUG_GRAPH
    fprintf(stderr," %dth edge points to:%s\n", i, e->head->name);
#endif
    if (e->head == head) {
      return e;
    }
  }
  return NULL;
}

/**
 * Simple BFS search to test dependence between two vertices
 */
int check_if_path_exists(vertex_t *origin, vertex_t *destination) {
  fifo_t *vertices_to_visit = fifo_new();
  fifo_write(vertices_to_visit, (void*) origin);
  while (!fifo_is_empty(vertices_to_visit)) {
    vertex_t *v = (vertex_t*) fifo_read(vertices_to_visit);
    for(int i=0; i<v->out_degree; i++) {
      vertex_t *u = v->out_edges[i]->head;
      if (u==destination) {
	fifo_free(vertices_to_visit);
	return 1;
      }
      fifo_write(vertices_to_visit, u);
    }
  }
  fifo_free(vertices_to_visit);
  return 0;
}
vertex_t * findVertexByName(graph_t* graph, std::string toFind){
    vertex_t *vertex = graph->first_vertex;

    while(vertex!= nullptr){
        std::string vname = vertex->name;
        std::transform(vname.begin(), vname.end(), vname.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        std::string vnameToFind = toFind;
        std::transform(vnameToFind.begin(), vnameToFind.end(), vnameToFind.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        if (vname==vnameToFind) return vertex;
        vertex = vertex->next;
    }
    return NULL;
}

vertex_t * findVertexById(graph_t* graph, int idToFind){
    vertex_t *vertex = graph->first_vertex;
    while(vertex!= nullptr){
        if (vertex->id==idToFind) return vertex;
        else vertex = vertex->next;
    }
    return  NULL;
}

double peakMemoryRequirementOfVertex(const vertex_t * v) {
    double maxMemReq = v->memoryRequirement;

    double sumIn = 0, sumOut = 0;

    for (int i = 0; i < v->in_degree; i++) {
        sumIn += v->in_edges[i]->weight;
    }
    for (int i = 0; i < v->out_degree; i++) {
        sumOut += v->out_edges[i]->weight;
    }
    if (sumIn > maxMemReq) {
        maxMemReq = sumIn;
     //   std::cout<<"Incoming over mi on "<<v->name<<std::endl;
    }
    if (sumOut > maxMemReq) {
        maxMemReq = sumOut;
       // std::cout<<"Outgoing over mi on "<<v->name<<std::endl;
}
    return maxMemReq;
}