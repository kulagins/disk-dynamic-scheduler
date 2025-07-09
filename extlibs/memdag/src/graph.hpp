#ifndef GRAPH_H
#define GRAPH_H
#include <assert.h>
// #include <igraph/igraph.h>
#include "vector"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

/**
 * \file graph.h
 * \brief graph definition, management and algorithms
 */

/** @name Useful macros */
///@{
#define max_memdag(x, y) (((x) > (y)) ? (x) : (y))
// #define min(x,y) (((x)<(y))?(x):(y))f
#define sign(x) ((x < 0) ? -1 : ((x > 0) ? 1 : 0))
#define MIN_PROCESSING_TIME 0.001
///@}
/**
 * Vertex type
 *
 * Vertices are organised in a double-linked list, hence the \p prev
 * and \p next pointers. Arrays \p in_edges and \p out_edges
 * contains pointers to the input and output edges. Their size is
 * given by \p in_size and \p out_size, however they contain only
 * \p in_degree and \p out_degree edges.
 *
 * \p data is a pointer reserved for the user's usage.

 */

enum Status { Unscheduled,
    Scheduled,
    // Blocked,
    //  Ready,
    Running,
    Finished
};

extern bool Debug;

struct graph_t;
struct vertex_t {
    /* basic vertex information */
    int id;
    std::string name;
    double time;
    /*only used in scheduler code. MemDag uses Liu's model, where node memory weights are expressed
     * as extra edge weights.
     * */
    double memoryRequirement = 0;
    double wchar = 0;
    double taskinputsize = 0;

    /* graph structure around the vertex */
    struct vertex_t* next = nullptr;
    struct vertex_t* prev = nullptr;
    int in_degree = 0;
    int out_degree = 0;
    std::unordered_map<int, struct edge*> in_edges;
    std::unordered_map<int, struct edge*> out_edges;

    ///\cond HIDDEN_SYMBOLS
    /* other data used for graph algorithms */
    int nb_of_unprocessed_parents = 0; // reserved for topological traversals
    int generic_int = 0;
    void* generic_pointer = nullptr;
    double top_level = -1;
    double bottom_level = -1;
    ///\endcond

    /* user data */
    void* data = nullptr;
    graph_t* subgraph = nullptr;
    int assignedProcessorId = -1;
    double makespan = -1;
    double makespanPerceived = -1;
    double makespanPerceivedHeft = -1;

    bool visited = false;
    Status status = Status::Unscheduled;

    double actuallyUsedMemory = -1;
    double factorForRealExecution = 1;

    double rank = -1;
};

/**
 * Enumeration of the various edge status.
 *
 *  The status of an edge can be either ORIGINAL (created by the
 *  user), IN_CUT (after computing the maximum cut with
 *  maximum_parallel_memory()), or ADDED (after limiting the memory
 *  with add_edges_to_cope_with_limited_memory())
 */

typedef enum e_edge_status_t { ORIGINAL = 0,
    IN_CUT,
    ADDED } edge_status_t;

/**
 * Edge type
 *
 * Edges are organised in a double-linked list, hence the
 * \p prev and \p next pointers. \p tail and \p head point to the
 * origin and destination vertices of the edge.
 *
 * \p data is a pointer reserved for the user's usage.
 */

enum class LocationType {
    OnProcessor,
    OnDisk,
    Nowhere
};

struct Location {
    LocationType locationType;
    std::optional<int> processorId; // Holds processor ID if location is OnProcessor
    std::optional<double> afterWhen;

    explicit Location(LocationType type, std::optional<int> procId = std::nullopt, std::optional<double> aftW = std::nullopt)
        : locationType(type)
        , processorId(procId)
        , afterWhen(aftW)
    {
    }
};

typedef struct edge {
    /* basic edge information */
    double weight = 0.0;

    /* graph structure */
    struct vertex_t* tail = nullptr;
    struct vertex_t* head = nullptr;
    struct edge* next = nullptr;
    struct edge* prev = nullptr;
    edge_status_t status;
    /* user data */
    void* data = nullptr;
    double factorForRealExecution = 1;
    ///\cond HIDDEN_SYMBOLS
    /* other data used for graph algorithms */
    void* generic_pointer = nullptr;
    std::vector<Location> locations;
    std::vector<Location> imaginedLocations;
    ///\endcond} edge_t;

    bool operator==(const edge& other) const
    {
        // std::cout <<"comparing "<<tail->name<<" -> "<<head->name<< " to "<< other.tail->name<<" -> "<<other.head->name<<'\n';
        return tail->name == other.tail->name && head->name == other.head->name && weight == other.weight;
    }

} edge_t;

/**
 * Type of a graph
 *
 * Contains first and last vertex (and edge) of each double-linked
 * list, as well as an array to quickly access a vertex given its id.
 *
 * The \p source and \p target vertices are set by
 * enforce_single_source_and_target() and used by other functions.
 */

struct graph_t {
    vertex_t* first_vertex = nullptr;
    edge_t* first_edge = nullptr;
    int next_vertex_index = -1;
    std::unordered_map<int, vertex_t*> vertices_by_id;
    vertex_t* source = nullptr;
    vertex_t* target = nullptr;
    int number_of_vertices = 0;
    int number_of_edges = 0;
    ///\cond HIDDEN_SYMBOLS
    int generic_vertex_pointer_lock = 0;
    int generic_vertex_int_lock = 0;
    int generic_edge_pointer_lock = 0;
    ///\endcond
};

///\cond HIDDEN_SYMBOLS
/* Macros to manage locks */
#define ACQUIRE(lock)      \
    {                      \
        assert(lock == 0); \
        lock = 1;          \
    }
#define RELEASE(lock) \
    {                 \
        lock = 0;     \
    }
///\endcond

/* From graph.c: */
graph_t* new_graph(void);
vertex_t* new_vertex(graph_t* graph, const std::string name, double time, void* data);
vertex_t* new_vertex2Weights(graph_t* graph, const std::string name, double time, double memRequirement, void* data);
edge_t* new_edge(graph_t* graph, vertex_t* tail, vertex_t* head, double weight, void* data);
void remove_vertex(graph_t* graph, vertex_t* v);
void remove_edge(graph_t* graph, edge_t* e);
graph_t* copy_graph(graph_t* graph, int reverse_edges);
void free_graph(graph_t* graph);

edge_t* find_edge(vertex_t* tail, vertex_t* head);
void enforce_single_source_and_target(graph_t* graph, std::string suffix = "");
void enforce_single_source_and_target_with_minimal_weights(graph_t* graph, std::string suffix = "");
graph_t* read_dot_graph(const char* filename, const char* memory_label, const char* timing_label, const char* node_memory_label);
void print_graph_to_dot_file(graph_t* graph, FILE* output);
void print_graph_to_cout(graph_t* graph);
void print_graph_to_cout_full(graph_t* graph);
// igraph_t  convert_to_igraph(graph_t *graph, igraph_vector_t *edge_weights_p, igraph_strvector_t *node_names_p, igraph_vector_t *vertex_times_p);
int check_if_path_exists(vertex_t* origin, vertex_t* destination);

/* From graph-algorithms.c: */
double compute_peak_memory(graph_t* graph, vertex_t** schedule);
std::vector<vertex_t*> compute_peak_memory_until(graph_t* graph, vertex_t** schedule, double maxMem, int& indexToStartFrom);
vertex_t* next_vertex_in_topological_order(graph_t* graph, vertex_t* vertex);
vertex_t* next_vertex_in_anti_topological_order(graph_t* graph, vertex_t* vertex);
void compute_bottom_and_top_levels(graph_t* graph);
void delete_transitivity_edges(graph_t* graph);
void remove_transitivity_edges_weight_conservative(graph_t* graph);
void merge_multiple_edges(graph_t* graph);

int sort_by_decreasing_bottom_level(const void* v1, const void* v2);
int sort_by_increasing_top_level(const void* v1, const void* v2);
int sort_by_increasing_avg_level(const void* v1, const void* v2);
vertex_t* next_vertex_in_sorted_topological_order(graph_t* graph, vertex_t* vertex, int (*compar)(const void*, const void*));

bool isLocatedNowhere(edge_t* edge, bool imaginary);
bool isLocatedOnDisk(edge_t* edge, bool imaginary);
bool isLocatedOnThisProcessor(edge_t* edge, int id, bool imaginary);
bool isLocatedOnAnyProcessor(edge_t* edge, bool imaginary);
int whatProcessorIsLocatedOn(edge_t* edge, bool imaginary);
void delocateFromThisProcessorToDisk(edge_t* edge, int id, bool imaginary, double afterWhen);
void delocateFromThisProcessorToNowhere(edge_t* edge, int id, bool imaginary, double afterWhen);
void locateToThisProcessorFromDisk(edge_t* edge, int id, bool imaginary, double afterWhen);
void locateToThisProcessorFromNowhere(edge_t* edge, int id, bool imaginary, double afterWhen);
void locateToDisk(edge_t* edge, bool imaginary, double afterWhen);

Location& getLocationOnProcessor(edge_t* edge, int id, bool imaginary);
Location& getLocationOnDisk(edge_t* edge, bool imaginary);

std::string buildEdgeName(edge_t* edge);

double getSumOut(vertex_t* v);
double getSumIn(vertex_t* v);
/** @name Macros to iterate over vertices*/
///@{
#define first_vertex(graph) (graph->first_vertex)
#define is_last_vertex(vertex) (vertex)
#define next_vertex(vertex) (vertex->next)
///@}

/* From maxmemory.c */

/** Possible edge selection heuristics */
typedef enum e_edge_selection_heuristic_t { MIN_LEVEL = 0,
    RESPECT_ORDER,
    MAX_SIZE,
    MAX_MIN_SIZE } edge_selection_heuristic_t;
double maximum_parallel_memory(graph_t* graph);
int add_edges_to_cope_with_limited_memory(graph_t* graph, double memory_bound, edge_selection_heuristic_t edge_selection_heuristic);

/* Added for the scheduler */

vertex_t* findVertexByName(graph_t* graph, std::string toFind);
vertex_t* findVertexById(graph_t* graph, int idToFind);
void print_edge(edge_t* v);
double peakMemoryRequirementOfVertex(const vertex_t* v);
double inMemoryRequirement(const vertex_t* v);
double outMemoryRequirement(const vertex_t* v);

class Swap {
private:
    vertex_t* firstTask;
    vertex_t* secondTask;
    double resultingMakespan;

public:
    Swap(vertex_t* f, vertex_t* s)
    {
        firstTask = f;
        secondTask = s;
        resultingMakespan = -1;
    }

    Swap(vertex_t* f, vertex_t* s, double ms)
    {
        firstTask = f;
        secondTask = s;
        resultingMakespan = ms;
    }

    void setMakespan(double ms)
    {
        resultingMakespan = ms;
    }

    vertex_t* getFirstTask()
    {
        return firstTask;
    }

    double getMakespan()
    {
        return resultingMakespan;
    }

    vertex_t* getSecondTask()
    {
        return secondTask;
    }

    void executeSwap();
    bool isFeasible();
};

struct workflowElement {
    std::variant<edge_t, vertex_t> element;
};
#endif
