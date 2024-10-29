#include <assert.h>
#include "restrict.hpp"

/*
 * Functions used to compute a sequential schedule sigma, which serves as a basis for the RespectOrder heuristic
 *
 */


// Compute a schedule following a BFS order for the given graph

void bfs(const igraph_t *graph, igraph_integer_t source, igraph_vector_t *rank)
{
    igraph_vector_t in_degrees;
    igraph_es_t es;
    igraph_t g;
    int j=0;
    igraph_dqueue_t queue;  //queue for the 0-indegree vertices
    igraph_vector_t neighbors;

    igraph_vector_init(&neighbors,0);
    igraph_vector_init(&in_degrees, 0);
    igraph_dqueue_init(&queue, 0);

    igraph_copy(&g, graph);
    //count the in-degree of vertices
    igraph_degree(&g, &in_degrees, igraph_vss_all(), IGRAPH_IN, IGRAPH_NO_LOOPS);

    igraph_dqueue_push(&queue,source);

    while( igraph_dqueue_size(&queue) != 0)
    {
        igraph_integer_t current_node = igraph_dqueue_pop(&queue);
        igraph_neighbors(&g, &neighbors, current_node, IGRAPH_OUT);

        int inDegree = VECTOR(in_degrees)[current_node];
        VECTOR(*rank)[j++] = current_node;
        for(int i=0; i < igraph_vector_size(&neighbors); i++)
        {
            int vertex= VECTOR(neighbors)[i];
            igraph_es_pairs_small(&es, IGRAPH_DIRECTED, current_node,  vertex, -1);
            igraph_delete_edges(&g, es);
            igraph_degree(&g, &in_degrees, igraph_vss_all(), IGRAPH_IN, IGRAPH_NO_LOOPS);

            int inDegree = VECTOR(in_degrees)[vertex];
            if(inDegree == 0)
            {
                igraph_dqueue_push(&queue, vertex);
            }
             igraph_es_destroy(&es);
        }

    }
    igraph_destroy(&g);
    igraph_dqueue_destroy(&queue);
    igraph_vector_destroy(&in_degrees);
    igraph_vector_destroy(&neighbors);

}


void mixBfsDfs(const igraph_t *graph, igraph_integer_t source, igraph_vector_t *rank, float *alfa)
{
    // alfa*DFS(i) + (1-alfa)*BFS(i)
    if( *alfa == 1.0f)
    {
        igraph_vector_t order;
        igraph_vector_init(&order,0);

        igraph_dfs(graph,
                   source,
                   IGRAPH_OUT, 0,
                   &order, rank,
                   NULL,
                   NULL, NULL,
                   NULL,
                   NULL);

        igraph_vector_reverse(rank);
       igraph_vector_destroy(&order);
    }else
        if(*alfa == 0.0f)
        {
            bfs(graph,source, rank);
        }
    else
    {
        igraph_vector_t rankDFS;
        igraph_vector_t rankBFS;
        igraph_vector_t order;

        igraph_vector_init(&order,0);
        igraph_vector_init(&rankDFS,0);
        igraph_vector_init(&rankBFS, igraph_vcount(graph));

        igraph_dfs(graph,
                   source,
                   IGRAPH_OUT, 0,
                   &order, &rankDFS,
                   NULL,
                   NULL, NULL,
                   NULL,
                   NULL);

        igraph_vector_reverse(&rankDFS);

        bfs(graph,source, &rankBFS);
        //helper vectors
        igraph_vector_t auxBFS;
        igraph_vector_t auxDFS;
        igraph_vector_t auxFinalRank;
        igraph_vector_init(&auxFinalRank,igraph_vcount(graph));
        igraph_vector_init(&auxBFS,igraph_vcount(graph));
        igraph_vector_init(&auxDFS,igraph_vcount(graph));
        //changing to BFS(i) = rank, because until now was BFS(rank)= i where i is a node
        for(int i=0; i < igraph_vcount(graph); i++){
            int nodeInBFS = VECTOR(rankBFS)[i];
            VECTOR(auxBFS)[nodeInBFS] = i;

            int nodeInDFS = VECTOR(rankDFS)[i];
            VECTOR(auxDFS)[nodeInDFS] = i;

        }


        for(int i=0; i < igraph_vcount(graph); i++){

            float r = *alfa * VECTOR(auxDFS)[i] + (1 - *alfa) * VECTOR(auxBFS)[i];
            VECTOR(auxFinalRank)[i] = r;
        }
        // transforming the rank into auxFinalRank(rank) = node from auxFinalRank(node) = rank
        // in order to have a sequence of scheduled nodes
        long int max =  igraph_vector_which_max(&auxFinalRank);
        for(int i=0; i < igraph_vcount(graph); i++){

                long int minim =  igraph_vector_which_min(&auxFinalRank);
                VECTOR(auxFinalRank)[minim] = max+1;
                VECTOR(*rank)[i] = minim;

        }
        igraph_vector_destroy(&order);
        igraph_vector_destroy(&rankBFS);
        igraph_vector_destroy(&rankDFS);
        igraph_vector_destroy(&auxBFS);
        igraph_vector_destroy(&auxDFS);
        igraph_vector_destroy(&auxFinalRank);
    }

}


void updateMaxPeak(igraph_real_t used_memory, igraph_real_t *peak_memory){

    if(used_memory > *peak_memory){
        *peak_memory = used_memory;
    }

}


void calculateMaxPeak(const igraph_t *graph, const igraph_vector_t *capacity, igraph_vector_t *rank,
igraph_real_t *max_peak)
{
    igraph_vector_t neighbors;
    igraph_vector_t neighbors_edgeIds;

    igraph_vector_init(&neighbors_edgeIds,0);
    igraph_vector_init(&neighbors,0);

    // rank(i) = node not rank(node) = rank
    igraph_real_t current_used_memory = 0;
    igraph_integer_t edgeId;

    for(int i=0; i < igraph_vector_size(rank); i++ )
    {
        igraph_real_t sum_input_capacities = 0;
        igraph_real_t sum_output_capacities = 0;

        igraph_integer_t current_node = VECTOR(*rank)[i];
        //input capacities from input neighbors
        igraph_neighbors(graph, &neighbors, current_node, IGRAPH_IN);
        for(int i=0; i< igraph_vector_size(&neighbors); i++)
        {
            igraph_integer_t neighbor_vertex = VECTOR(neighbors)[i];
            igraph_get_eid(graph, &edgeId, neighbor_vertex, current_node, IGRAPH_DIRECTED, 1);
            igraph_real_t input_neighborEdge_capacity = VECTOR(*capacity)[edgeId];
            sum_input_capacities += input_neighborEdge_capacity;
        }
        // output neighbors capacities
        igraph_neighbors(graph, &neighbors, current_node, IGRAPH_OUT);

        for(int i=0; i < igraph_vector_size(&neighbors); i++)
        {
            igraph_integer_t neighbor_vertex = VECTOR(neighbors)[i];
            igraph_get_eid(graph, &edgeId,  current_node, neighbor_vertex, IGRAPH_DIRECTED, 1);
            igraph_real_t output_neighborEdge_capacity = VECTOR(*capacity)[edgeId];
            sum_output_capacities += output_neighborEdge_capacity;

        }
        current_used_memory -= sum_input_capacities;
        current_used_memory += sum_output_capacities;

        updateMaxPeak(current_used_memory, max_peak);

    }
    igraph_vector_destroy(&neighbors);
    igraph_vector_destroy(&neighbors_edgeIds);

}



int peak_exceeds_memory(const igraph_t *graph, const igraph_vector_t *capacity, igraph_vector_t *rank,
                igraph_real_t memory_bound, igraph_real_t *max_peak){

    calculateMaxPeak(graph, capacity, rank, max_peak);
    if( *max_peak > memory_bound){
    return 0; //not succeeded
    }
    return 1;
}


int sigma_schedule(const igraph_t *graph, igraph_integer_t source, igraph_vector_t *rank ,const igraph_vector_t *capacity,
                igraph_real_t memory_bound){
    float alfa = 0.0f;
    int success = 0;
    
    igraph_vector_t rank_inv;
    igraph_vector_init(&rank_inv, igraph_vcount(graph));
    
    while(alfa <= 1.04f  &&  success == 0){
        igraph_real_t max_peak = 0 ;
        mixBfsDfs(graph, source, &rank_inv, &alfa);
        success = peak_exceeds_memory(graph, capacity, &rank_inv, memory_bound, &max_peak);
        alfa += 0.05f;
    }
    
    
    // BS: reversed the vector rank
    for (int i=0; i<igraph_vector_size(rank); i++)
    {
        int k = VECTOR(rank_inv)[i];
        assert(k<igraph_vector_size(rank));
        VECTOR(*rank)[k] = i;
//        VECTOR(*rank)[i] = k;
    }
    
    
    igraph_vector_destroy(&rank_inv);
    return success;
}


