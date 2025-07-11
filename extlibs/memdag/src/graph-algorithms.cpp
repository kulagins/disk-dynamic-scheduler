#include "fifo.hpp"
#include "graph.hpp"
#include "heap.hpp"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

// #define DEBUG_GRAPH_ALGO

/**
 * \file graph-algorithms.c
 * \brief Traverals and other tools on graphs
 */

/**
 * Computes the maximal memory of a schedule
 * @param graph the graph
 * @param schedule schedule given as an array of vertex pointer, whose size if equal to the number of vertices in the graph
 */
double compute_peak_memory(graph_t* graph, vertex_t** schedule)
{
    double current_memory = 0.0;
    double peak_memory = 0.0;
    for (int i = 0; i < graph->number_of_vertices; i++) {
        vertex_t* v = schedule[i];
        double mem_increment = 0.0;
        for (const auto* e : v->in_edges) {
            mem_increment -= e->weight;
        }
        for (const auto* e : v->out_edges) {
            mem_increment += e->weight;
        }
        current_memory += mem_increment;
        if (current_memory > peak_memory)
            peak_memory = current_memory;
    }
    return peak_memory;
}

std::vector<vertex_t*> compute_peak_memory_until(graph_t* graph, vertex_t** schedule, double maxMem, int& indexToStartFrom)
{
    double current_memory = 0.0;

    std::vector<vertex_t*> schedulePart;
    for (int i = indexToStartFrom; i < graph->number_of_vertices; i++) {
        vertex_t* v = schedule[i];
        double mem_increment = 0.0;
        for (const auto* e : v->in_edges) {
            mem_increment -= e->weight;
        }
        for (const auto* e : v->out_edges) {
            mem_increment += e->weight;
        }
        current_memory += mem_increment;
        if (current_memory > maxMem) {
            indexToStartFrom = i;
            return schedulePart;
        } else {
            schedulePart.emplace_back(v);
            indexToStartFrom = i;
        }
    }
    indexToStartFrom++;
    return schedulePart;
}

static fifo_t* ready_vertices = NULL;

/**
 * Topological traversal of a graph
 * @param graph the graph
 * @param vertex the last vertex seen (NULL on first call)
 *
 * \return the next vertex to visit (NULL if all graph was visited)
 */
vertex_t* next_vertex_in_topological_order(graph_t* graph, vertex_t* vertex)
{
    // First call, initialize data
    if (ready_vertices == NULL) {
        ready_vertices = fifo_new();
        for (vertex_t* u = graph->first_vertex; u; u = u->next) {
            u->nb_of_unprocessed_parents = u->in_edges.size();
        }
    }
    // If called without any vertex, return the source node
    if (!vertex) {
        return graph->source;
    }
    // Otherwise, process successors of current node
    for (const auto* edge: vertex->out_edges) {
        vertex_t* succ = edge->head;
        succ->nb_of_unprocessed_parents--;
        if (succ->nb_of_unprocessed_parents == 0) {
            fifo_write(ready_vertices, (void*)succ);
        }
    }

    // If no more ready vertices, clean up
    if (fifo_is_empty(ready_vertices)) {
        fifo_free(ready_vertices);
        ready_vertices = NULL;
        return NULL;
    }
    // Otherwise, return an available vertex
    return (vertex_t*)fifo_read(ready_vertices);
}

static heap_t* ready_vertices_sorted = NULL;
/**
 *Topological traversal of a graph, sorting available vertices
 * @param graph the graph
 * @param vertex the last vertex seen (NULL on first call)
 * @param compar function to compare available vertives
 *
 * \return the next vertex to visit (NULL if all graph was visited)
 */
vertex_t* next_vertex_in_sorted_topological_order(graph_t* graph, vertex_t* vertex, int (*compar)(const void*, const void*))
{ // use nb_of_unprocessed_parents
    // First call, initialize data
    if (ready_vertices_sorted == NULL) {
        ready_vertices_sorted = heap_new(compar);
        for (vertex_t* u = graph->first_vertex; u; u = u->next) {
            u->nb_of_unprocessed_parents = u->in_edges.size();
        }
    }
    // If called without any vertex, return the source node
    if (!vertex) {
        return graph->source;
    }
    // Otherwise, process successors of current node
    for (auto* edge : vertex->out_edges) {
        vertex_t* succ = edge->head;
        succ->nb_of_unprocessed_parents--;
        if (succ->nb_of_unprocessed_parents == 0) {
            heap_push(ready_vertices_sorted, succ);
        }
    }

    // If no more ready vertices, clean up
    if (ready_vertices_sorted->count == 0) {
        heap_free(ready_vertices_sorted);
        ready_vertices_sorted = NULL;
        return NULL;
    }
    // Otherwise, return an available vertex
    return (vertex_t*)heap_pop(ready_vertices_sorted);
}

/**
 *Traversal of a graph, in reverse topological order
 * @param graph the graph
 * @param vertex the last vertex seen (NULL on first call)
 *
 * \return the next vertex to visit (NULL if all graph was visited)
 */

vertex_t* next_vertex_in_anti_topological_order(graph_t* graph, vertex_t* vertex)
{
    // First call, initialize data
    if (ready_vertices == NULL) {
        ready_vertices = fifo_new();
        for (vertex_t* u = graph->first_vertex; u; u = u->next) {
            u->nb_of_unprocessed_parents = u->out_edges.size();
        }
    }
    // If called without any vertex, return the target node
    if (!vertex) {
        return graph->target;
    }
    // Otherwise, process predecessors of current node
    for (int i = 0; i < vertex->in_edges.size(); i++) {
        vertex_t* pred = vertex->in_edges[i]->tail;
        pred->nb_of_unprocessed_parents--;
        if (pred->nb_of_unprocessed_parents == 0) {
            fifo_write(ready_vertices, (void*)pred);
        }
    }

    // If no more ready vertices, clean up
    if (fifo_is_empty(ready_vertices)) {
        fifo_free(ready_vertices);
        ready_vertices = NULL;
        return NULL;
    }
    // Otherwise, return an available vertex
    return (vertex_t*)fifo_read(ready_vertices);
}

/**
 * Compute the bottom level and top level of all nodes, and store them in the graph data structure
 */
void compute_bottom_and_top_levels(graph_t* graph)
{
    // Top-levels: topological traversal from the source
    graph->source->top_level = 0;
    for (vertex_t* u = graph->source; u; u = next_vertex_in_topological_order(graph, u)) {
        u->top_level = 0;
        for (int i = 0; i < u->in_edges.size(); i++) {
            vertex_t* v = u->in_edges[i]->tail; // egde v->u
            u->top_level = max_memdag(u->top_level, v->top_level + v->time);
        }
    }
    // Bottom-levels: anti-topological traversal from the target

    graph->target->bottom_level = graph->target->time;
    for (vertex_t* u = graph->target; u; u = next_vertex_in_anti_topological_order(graph, u)) {
        u->bottom_level = 0;
        for (int i = 0; i < u->out_edges.size(); i++) {
            vertex_t* v = u->out_edges[i]->head; // edge u->v
            u->bottom_level = max_memdag(u->bottom_level, v->bottom_level);
        }
        u->bottom_level += u->time;
    }
}

///\cond HIDDEN_SYMBOLS
#define longest_path_length(u) (u->generic_int)
#define pred_vertex_on_longest_path(u) ((vertex_t*)u->generic_pointer)
#define set_pred_vertex_on_longest_path(u, v) (u->generic_pointer = (void*)v)
///\endcond

static void compute_longest_path_from_vertex(graph_t* graph, vertex_t* vertex)
{ // Warning: uses generic_int and generic_pointer !
    /* Locks should hae been taken before */
    assert(graph->generic_vertex_int_lock);
    assert(graph->generic_vertex_pointer_lock);

    for (vertex_t* u = graph->first_vertex; u; u = u->next) {
        longest_path_length(u) = -1;
        set_pred_vertex_on_longest_path(u, NULL);
    }
    for (vertex_t* u = graph->source; u; u = next_vertex_in_topological_order(graph, u)) {
        if (u == vertex) {
            longest_path_length(u) = 0;
        } else {
            double max_length_predecessors = -1;
            vertex_t* max_length_pred_vertex = NULL;
            for (int i = 0; i < u->in_edges.size(); i++) {
                vertex_t* v = u->in_edges[i]->tail;
                if (max_length_predecessors < longest_path_length(v)) {
                    max_length_predecessors = longest_path_length(v);
                    max_length_pred_vertex = v;
                }
            }
            if (max_length_predecessors == -1) {
                longest_path_length(u) = -1;
            } else {
                longest_path_length(u) = max_length_predecessors + 1;
                set_pred_vertex_on_longest_path(u, max_length_pred_vertex);
            }
        }
    }
#ifdef DEBUG_GRAPH_ALGO
    fprintf(stderr, "At the end of compute_longest_path_from_vertex from vertex %s:\n", vertex->name);
    for (vertex_t* u = graph->first_vertex; u; u = u->next) {
        fprintf(stderr, "   vertex %s  \tis at length %d   \tpredecessor on path:%s\n",
            u->name, longest_path_length(u),
            (pred_vertex_on_longest_path(u) == NULL) ? "(null)" : pred_vertex_on_longest_path(u)->name);
    }
#endif
}

/**
 * Remove transitivity edges from a graph
 */
void delete_transitivity_edges(graph_t* graph)
{ // Warning: uses generic_int and generic_pointer ! (because of compute_longest_path_from_vertex)
    fifo_t* edges_to_be_suppressed = fifo_new();
    ACQUIRE(graph->generic_vertex_int_lock);
    ACQUIRE(graph->generic_vertex_pointer_lock);

    for (vertex_t* u = graph->first_vertex; u; u = u->next) {
        compute_longest_path_from_vertex(graph, u);
        for (int i = 0; i < u->out_edges.size(); i++) {
            vertex_t* v = u->out_edges[i]->head;
#ifdef DEBUG_GRAPH_ALGO
            fprintf(stderr, "Longest path from %s to %s:%d\n", u->name, v->name, longest_path_length(v));
#endif
            if (longest_path_length(v) > 1) {
                fifo_write(edges_to_be_suppressed, (void*)u->out_edges[i]);
#ifdef DEBUG_GRAPH_ALGO
                fprintf(stderr, "Scheduling removal of transitive edge %s->%s (longest path length:%d)\n", u->name, v->name, longest_path_length(v));
#endif
            }
        }
    }

    while (!fifo_is_empty(edges_to_be_suppressed)) {
        edge_t* e = (edge_t*)fifo_read(edges_to_be_suppressed);
#ifdef DEBUG_GRAPH_ALGO
        fprintf(stderr, "Removing edge %s->%s\n", e->tail->name, e->head->name);
#endif
        remove_edge(graph, e);
    }
    fifo_free(edges_to_be_suppressed);
    RELEASE(graph->generic_vertex_int_lock);
    RELEASE(graph->generic_vertex_pointer_lock);
}

/**
 * Remove transitivity edges from a graph but conserves memory weights
 *
 * Whenever an edge is removed as it can be deducted from a path, the
 * weight of the removed edge is added to the weight of all edges in
 * the path.
 */

void remove_transitivity_edges_weight_conservative(graph_t* graph)
{ // Warning: uses generic_int and generic_pointer ! (because of compute_longest_path_from_vertex)
    fifo_t* edges_to_be_suppressed = fifo_new();
    ACQUIRE(graph->generic_vertex_int_lock);
    ACQUIRE(graph->generic_vertex_pointer_lock);

    for (vertex_t* u = graph->first_vertex; u; u = u->next) {
        compute_longest_path_from_vertex(graph, u);
        for (int i = 0; i < u->out_edges.size(); i++) {
            vertex_t* v = u->out_edges[i]->head;
            if (longest_path_length(v) > 1) {
                fifo_write(edges_to_be_suppressed, (void*)u->out_edges[i]);
#ifdef DEBUG_GRAPH_ALGO
                fprintf(stderr, "Scheduling removal of transitive edge %s->%s (longest path length:%d)\n", u->name, v->name, longest_path_length(v));
#endif
            }
        }

        while (!fifo_is_empty(edges_to_be_suppressed)) {
            edge_t* e = (edge_t*)fifo_read(edges_to_be_suppressed);
#ifdef DEBUG_GRAPH_ALGO
            fprintf(stderr, "Removing edge %s->%s, keeping weight\n", e->tail->name, e->head->name);
#endif
            vertex_t* target_tail = e->tail;
            vertex_t* current_vertex = e->head;
            while (current_vertex != target_tail) {
                vertex_t* current_tail = pred_vertex_on_longest_path(current_vertex);
#ifdef DEBUG_GRAPH_ALGO
                fprintf(stderr, "   Adding weight %f to edge %s->%s\n", e->weight, current_tail->name, current_vertex->name);
#endif
                find_edge(current_tail, current_vertex)->weight += e->weight;
                current_vertex = current_tail;
            }
            remove_edge(graph, e);
        }
    }
    fifo_free(edges_to_be_suppressed);
    RELEASE(graph->generic_vertex_int_lock);
    RELEASE(graph->generic_vertex_pointer_lock);
}

/**
 * Transform a multigraph into a graph by conserving memory weights
 *
 * If several edges go from a single source to a single target, they
 * are replaced by a single edge with cumulative weight.
 */
void merge_multiple_edges(graph_t* graph)
{ // Warning: uses generic_pointer !
    ACQUIRE(graph->generic_vertex_pointer_lock);

    for (vertex_t* v = graph->first_vertex; v; v = v->next) {
        v->generic_pointer = NULL; // points to edge to last considered vertex (current one or a previous one)
    }
    fifo_t* edges_to_be_suppressed = fifo_new();

    for (vertex_t* v = graph->first_vertex; v; v = v->next) {
        for (int i = 0; i < v->out_edges.size(); i++) {
            edge_t* current_edge = v->out_edges[i];
            vertex_t* current_target = current_edge->head;
            edge_t* old_edge = (edge_t*)current_target->generic_pointer;
            if ((old_edge) && (old_edge->tail == v)) { // New edge to a vertex already seen in v's successors
#ifdef DEBUG_GRAPH_ALGO
                fprintf(stderr, "Detected other copy of edge %s->%s, of weight %f (cumulated to existing weight %f), scheduled for removal\n",
                    v->name, current_target->name, current_edge->weight, old_edge->weight);
#endif
                // Add new weight to old edge
                old_edge->weight += current_edge->weight;

                // Mark edge for removal
                fifo_write(edges_to_be_suppressed, (void*)current_edge);

            } else { // Edge to new successor of v
                current_target->generic_pointer = (void*)current_edge;
            }
        }
    }

    while (!fifo_is_empty(edges_to_be_suppressed)) {
        edge_t* e = (edge_t*)fifo_read(edges_to_be_suppressed);
#ifdef DEBUG_GRAPH_ALGO
        fprintf(stderr, "Removing one copy of edge %s->%s\n", e->tail->name, e->head->name);
#endif
        remove_edge(graph, e);
    }
    fifo_free(edges_to_be_suppressed);
    RELEASE(graph->generic_vertex_pointer_lock);
}

/**
 * Vertex comparison function (for next_vertex_in_sorted_topological_order())
 */
int sort_by_decreasing_bottom_level(const void* v1, const void* v2)
{
    return (((vertex_t*)v1)->bottom_level >= ((vertex_t*)v2)->bottom_level);
}
/**
 * Vertex comparison function (for next_vertex_in_sorted_topological_order())
 */

int sort_by_increasing_top_level(const void* v1, const void* v2)
{
    return (((vertex_t*)v1)->top_level <= ((vertex_t*)v2)->top_level);
}
/**
 * Vertex comparison function (for next_vertex_in_sorted_topological_order())
 */

int sort_by_increasing_avg_level(const void* v1, const void* v2)
{
    return ((((vertex_t*)v1)->top_level - ((vertex_t*)v1)->bottom_level) <= (((vertex_t*)v2)->top_level - ((vertex_t*)v2)->bottom_level));
}
