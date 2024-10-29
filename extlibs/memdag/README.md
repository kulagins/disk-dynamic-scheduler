# memDAG #

## Summary ##

This library provides tools for handling Directed Acyclic Graphs
(DAGs), such as DAGs describing workflows and task graphs. Graph
vertices (or tree nodes) usually represent tasks whereas edges
represent their dependences, in the form of input and ouput data. The
main focus of this library is on the memory (or storage) needed to
traverse of these graphs, or equivalently, to process the
workflow/task graph.  It also includes some generic tools for graphs,
and in particular for the recognition of Series-Parallel (SP) graphs,
as well as the transformation of a graph into a SP-graph by adding
synchronisation vertices (also known as SP-ization). It may be used
either as a library, or as a command-line program (see `memdag
--help`).

Available features currently include:

- Reading graphs from various input formats (dot, list of parents for trees)
- Transformation of the graph into a SP-graph by adding synchronisation vertices
- Computation of the minimum memory to traverse the graph:
	- Optimal traversal and best post-order traversal for trees by Liu
	- Optimal traversal for SP-graphs (from [Kayaaslan18])
 - Computation of the maximum memory for a parallel processing (from [Marchal18])
 - Limitation of the maximum parallel memory by adding synchronisation edges (from [Marchal18]
 

This codes depends on:

 - 'graphviz' for graph input/output in the dot format (tested with
version 2.40.1 and 5.0.0)
 - 'igraph' for the minflox/maxcut algorithm, needed to compute the
   maximum parallel memory (tested with versions 0.7.1 and 0.9.9,
   incompatible with version 0.10.4) 

Note: when you configure igraph (>=0.8), it may use local library for
BLAS, LAPACK, GLPK, etc. Then, you have to add this library to LIBS
and LDFLAGS in order to build memdag. Another solution is to force
igraph to use internal methods only, for example by calling cmake (in
the build directory) with:

    cmake ..\
        -DIGRAPH_USE_INTERNAL_BLAS=ON \
        -DIGRAPH_USE_INTERNAL_LAPACK=ON \
        -DIGRAPH_USE_INTERNAL_ARPACK=ON \
        -DIGRAPH_USE_INTERNAL_GLPK=ON \
        -DIGRAPH_USE_INTERNAL_GMP=ON


It has been tested both on Ubuntu (Ubuntu 20.04 with gcc 9.4.0) and
Mac OS X with clang 11.0.0).


## Basic usage ##

Here is a quick description of the capabilities of this code. For more
information, see the included documentation in the `doc/`directory.

### Memory Models ###

Two classical memory models are available for input graphs or
trees. The first one, denoted as Liu's model in the following, comes
from the seminal paper [Liu87]. We later motivate its use by showing
it is expressive enough to emulate many other models
[Marchal18]. Memory weights are only present on edges. At the
beginning of the processing of a task, the inputs of the task are
immediately freed and the output allocated. Thus, the memory footprint
of the task is equal to the memory footprint of its output.

The second model, more general, has been proposed in [Jacquelin11]
(and is thus called the IPDPS'11 model). In this model, both edges and
vertices have a memory weight. In the beginning of the processing of a
task, the output data (corresponding to the edge weight) as well as
the temporary data of the task (corresponding to the vertex weight)
are allocated. The input data as well as the temporary data are freed
at the termination of the task.

### Reading and Writing Graphs or Trees ###

The `read_graph` function is the basic input function, able to read a
graph described in dot format. The user can provided the label of the
memory weight of edges through the `memory_label`parameter and the
processing time of nodes through the `timing_label`parameter. The last
parameter `node_processing_label` is optional: if used, the IPDPS'11
model is used and this denodes the label of the node processing
weight. Otherwise, Liu's model is used. Graphs
may be written to disk using the `print_graph_to_dot_file`function.


A special function allow to read trees as a list of nodes:
`read_tree_as_node_list`. The expected format depends on the memory
model. For the IPDPS'11 model, it is:
`<node_id> <parent_id> <node_processing_size> <output_size> <processing_time>`
whereas for Liu's model it is:
`<node_id> <parent_id> <output_size> <processing_time>`
In both cases `# <node_name>`can be added at the end of each
line. Node ids are supposed to start at 1, and id 0 denotes the root
node. Trees can be output either as dot files or list of nodes using
`print_dot_tree`or `print_tree_as_node_list`.

Trees can be transformed to graphs using `tree_to_graph`, and graphs
corresponding to trees can be transformed into trees using
`graph_to_tree`.


### Modifying Graphs ###

Graphs can be copied, modified and deleted using the following functions:
`new_vertex`, `new_edge`, `remove_vertex`, `remove_edge`,
`copy_graph`, `free_graph`.

It is easy to iterate through graph vertices using the macros
`first_vertex`, `next_vertex` and `is_last_vertex`.

### Memory of a Schedule, Maximum Memory of a Graph ###

Given a sequential schedule, `compute_peak_memory` allows to compute
its peak memory. Source files `graph-algorithms.c` and `graph.c`
contain several useful function to schedule and sort graph vertices.

The `maximum_parallel_memory`function compute the largest possible
peak memory of any parallel schedule (with unbounded number of
processor). To limit this maximum memory, one may use
`add_edges_to_cope_with_limited_memory` (see [Marchal18]).

### Memory of Trees ###

Two function are provided to compute the memory needed to
(sequentially) process a tree:

- `Liu_optimal_sequential_memory` to compute the minimum required
memory (and possibly the corresponding schedule) (see [Liu87])
- `best_postorder_sequential_memory` to compute the minimum memory of
  a postorder schedule (and possibly the corresponding schedule) (see [Liu86]).

### SP-graphs ###

Series-Parallel (SP) graphs are recursively defined using either a
Series or a Parallel composition. We implemented in function
`build_SP_decomposition_tree` the SP decomposition algorithm proposed
by Valdes, Tarjan and Lawler [Valdes82]. It may be used to test
whether a graph is a SP-graph or not. The algorithm can be summarized
as follows:

1. Maintain a list of *unsatisfied vertices* (vertices with some work to do). In the beginning, it contains all vertices except source and sink (which are never added)
2. Pick (remove) a vertex in this list
  1. Perform as many parallel reductions as possible on edges incident to this vertex
  2. If the vertex has only one input edge and one output edge,
     perform a series reduction, add its neighbors to the unsatisfied
     list (if there are not already present and there are not source
     or sink)
3. Perform parallel reductions on edges from source to sink.

An algorithm is provided that converts any DAG into a Series-Parallel
Graph, available in the `graph_sp_ization` function: synchronisation
vertices and edges are added to ensure that all dependencies of the
original graphs are kept, while the resulting graph structure is
Series-Parallel. The algorithm is adapted from [González-Escribano02].

The optimal (minimal) memory required by any sequential processing of
a SP-graph may be computed using `compute_optimal_SP_traversal`
(algorithm described in [Kayaaslan18]).

## Some Internal Specificities ##

### Computation Model ###

Internally, we use Liu's memory model, as specified in the JPDC'18 paper: at the
beginning of the computation of a task, its inputs are instantaneously
replaced by its output. As precised in the paper, this model is
powerful enough to emulate other model. In particular, in the
read\_dot\_graph function, the user may precise a node memory label. If
this is provided, we change the computation model for the one of the
IPDPS'11 paper: during a task's computation, its inputs, its outputs
and its temporary data are stored in memory.

### Graph and Tree Formats ###

The internal format for storing graphs and trees are described in
`graph.h` and `tree.h`. Graph vertices and edges are stored both as an
array and a double-linked lists. Tree nodes are stored in an array and
linked in a recursive structure (pointers to parent and
children). Graph vertices and edges have generic `data`pointers
intended for the user that are not used by the library. On the
contrary, the `generic_int`and `generic_pointer` fields are used by
the library and protected by locks. They should not be used unless
well understood.

### Data Structures ###

For now, custom implementations of heap and FIFO datastructures are used,
however, there may some day be replaced by the C++ standard libary.

### Documentation and Tests ###

The documentation is provided by Doxygen, under the `doxygen`
directory.

Regression tests are provided and the code coverage can be checked
using gcov/lcov.

## References ##

- [González-Escribano02] González-Escribano, A., Van Gemund, A. J., & Cardeñoso-Payo, V. (2002, June). *Mapping Unstructured Applications into Nested Parallelism.* In International Conference on High Performance Computing for Computational Science (pp. 407-420). Springer, Berlin, Heidelberg.
- [Jacquelin11] Jacquelin, M., Marchal, L., Robert, Y., & Uçar, B. (2011, May). *On optimal tree traversals for sparse matrix factorization.* In 2011 IEEE International Parallel & Distributed Processing Symposium (pp. 556-567). IEEE.
- [Kayaaslan18] Kayaaslan, E., Lambert, T., Marchal, L., & Uçar, B. (2018). *Scheduling series-parallel task graphs to minimize peak memory.* Theoretical Computer Science, 707, 1-23.
- [Liu86] Liu, J. W. (1986). *On the storage requirement in the out-of-core multifrontal method for sparse factorization.* ACM Transactions on Mathematical Software (TOMS), 12(3), 249-264.
- [Liu87] Liu, J. W. (1987). *An application of generalized tree pebbling to sparse matrix factorization.* SIAM Journal on Algebraic Discrete Methods, 8(3), 375-395.
- [Marchal18] Marchal, L., Simon, B., & Vivien, F. (2019). *Limiting the memory footprint when dynamically scheduling DAGs on shared-memory platforms.* Journal of Parallel and Distributed Computing, 128, 30-42.
- [Valdes82] Valdes, J., Tarjan, R. E., & Lawler, E. L. (1982). The recognition of series parallel digraphs. SIAM Journal on Computing, 11(2), 298-313.

