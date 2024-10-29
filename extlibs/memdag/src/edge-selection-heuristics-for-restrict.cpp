#include <assert.h>
#include "restrict.hpp"


int have_no_path(const igraph_t *graph, igraph_integer_t from, igraph_integer_t to)
{

    igraph_vector_t path_vertices;
    igraph_vector_init(&path_vertices, 0);
    fprintf(stderr,"Shortest path in have_no_path... ");
    igraph_get_shortest_path(graph, &path_vertices, 0, from, to, IGRAPH_OUT);
    if(igraph_vector_size(&path_vertices) != 0 )
      {
	igraph_vector_destroy(&path_vertices);
	fprintf(stderr," Done (path exists).\n");
        return 0; // not good
    }
    else
    {
	igraph_vector_destroy(&path_vertices);
	fprintf(stderr," Done (path doesn't exist).\n");
        return 1; // has no path, good
    }

}



int respectOrder(graph_t *original_graph, const igraph_t *graph, igraph_vector_t *cut, igraph_vector_t *rank, igraph_vector_t *nodework,
                  igraph_vector_t *capacity, igraph_integer_t source, igraph_integer_t target,
                  igraph_integer_t *selected_head_vertex, igraph_integer_t *selected_tail_vertex){
    igraph_integer_t a,b,c,d;
    int it_contains_first = 0;
    int it_contains_last = 0;

    *selected_head_vertex = *selected_tail_vertex = -1;

    int i=0;
    
    // loop over the cut
    while(i < igraph_vector_size(cut))
    {
        
        igraph_integer_t edge = VECTOR(*cut)[i];
        igraph_edge(graph,edge, &a, &b);
        i++;

        // useless to remove an empty edge from the cut
        if (VECTOR(*capacity)[edge] == 0)
        {
            continue;                
        }

        // if we find better nodes, select them        
        if(*selected_tail_vertex <0 || VECTOR(*rank)[a] > VECTOR(*rank)[*selected_tail_vertex])
        {
            *selected_tail_vertex = a;
        }
        if(*selected_head_vertex <0 || VECTOR(*rank)[b] < VECTOR(*rank)[*selected_head_vertex])
        {
            *selected_head_vertex = b;
        }
    }
    
assert(*selected_head_vertex>-1 && *selected_tail_vertex>-1);
//    assert(have_no_path(graph,*selected_tail_vertex,*selected_head_vertex));

 return 1;
}




//longest length path from the source to the target
void top_level_2(const igraph_t *graph, igraph_integer_t source, igraph_vector_t *cut, igraph_vector_t *toplevels,
        igraph_vector_t *vertices, igraph_vector_t *nodework)
{
    igraph_integer_t u,v;

    int size = 0;

    igraph_vs_t vs_source;
    igraph_vs_vector_small(&vs_source, source, -1);

    igraph_matrix_t res;
    igraph_matrix_init(&res, 0, 0);

    for(int i = 0; i < igraph_vector_size(cut); i++)
    {
        igraph_integer_t edge = VECTOR(*cut)[i];
        igraph_edge(graph,edge, &u, &v);

        if(!igraph_vector_contains(vertices, v))
        {
            size++;
            igraph_vector_resize(vertices, size);
            igraph_vector_resize(toplevels, size);
            VECTOR(*vertices)[size-1] = v;
        }
    }
    igraph_vs_t vs;
    igraph_vs_vector(&vs, vertices);
    igraph_shortest_paths_bellman_ford(graph, &res, vs_source, vs, nodework, IGRAPH_OUT);
    for(int i=0; i< igraph_vector_size(vertices); i++){
        igraph_integer_t node = VECTOR(*vertices)[i];
        VECTOR(*toplevels)[i] = -MATRIX(res, 0, i);
    }
    igraph_matrix_destroy(&res);

	igraph_vs_destroy(&vs_source);
	igraph_vs_destroy(&vs);
}

void bottom_level_2(const igraph_t *graph, igraph_integer_t target, igraph_vector_t *cut, igraph_vector_t *bottomlevels,
                     igraph_vector_t *vertices, igraph_vector_t *nodework, igraph_vector_t *true_nodework)
{
    igraph_integer_t u,v;
    int size = 0;
    igraph_vs_t vs_target;
    igraph_vs_vector_small(&vs_target, target, -1);
    igraph_matrix_t res;
    igraph_matrix_init(&res, 0, 0);

    for(int i = 0; i < igraph_vector_size(cut); i++)
    {
        igraph_integer_t edge = VECTOR(*cut)[i];
        igraph_edge(graph,edge, &u, &v);

        if(!igraph_vector_contains(vertices, u))
        {
            size++;
            igraph_vector_resize(vertices, size);
            igraph_vector_resize(bottomlevels, size);
            VECTOR(*vertices)[size-1] = u;
        }
    }
    igraph_vs_t vs;
    igraph_vs_vector(&vs, vertices);
    igraph_shortest_paths_bellman_ford(graph, &res, vs, vs_target, nodework, IGRAPH_OUT);
    for(int i=0; i< igraph_vector_size(vertices); i++){
        igraph_integer_t node = VECTOR(*vertices)[i];
        VECTOR(*bottomlevels)[i] = -MATRIX(res, i, 0) + VECTOR(*true_nodework)[node];
    }

    igraph_matrix_destroy(&res);

	igraph_vs_destroy(&vs_target);
	igraph_vs_destroy(&vs);

}


int minlevels_2(graph_t *original_graph, const igraph_t *graph, igraph_vector_t *cut, igraph_vector_t *rank, igraph_vector_t *nodework,
                igraph_vector_t *capacity, igraph_integer_t source, igraph_integer_t target,
                igraph_integer_t *selected_head_vertex, igraph_integer_t *selected_tail_vertex)
{
    igraph_vector_t aux_nodework;
    igraph_vector_init(&aux_nodework, igraph_vector_size(capacity));

    for(int i=0; i< igraph_vector_size(capacity); i++){
        igraph_integer_t a,b;
        igraph_edge(graph, i, &a, &b);
        igraph_real_t nodew = VECTOR(*nodework)[b];
        VECTOR(aux_nodework)[i] = nodew;
}
    igraph_vector_scale(&aux_nodework, -1);
    igraph_vector_t toplevels, vertices_top, vertices_bottom;
    igraph_vector_init(&toplevels, 0);
    igraph_vector_init(&vertices_top, 0);
    igraph_vector_init(&vertices_bottom, 0);
    top_level_2(graph, source, cut, &toplevels, &vertices_top, &aux_nodework);

    igraph_vector_t bottomlevels;
    igraph_vector_init(&bottomlevels, 0);

    bottom_level_2(graph, target, cut, &bottomlevels, &vertices_bottom, &aux_nodework, nodework);

    int i = 0;
    int found_low_bottom = 0;
    int found_low_top = 0;
    igraph_vector_t aux_bottomlevels;
    igraph_vector_init(&aux_bottomlevels, igraph_vector_size(&bottomlevels));
    igraph_integer_t a,b,c,d;

    while((i <  (igraph_vector_size(&toplevels))) && found_low_top == 0)
    {
        long int low_toplevel_index =  igraph_vector_which_min(&toplevels);
        igraph_vector_update(&aux_bottomlevels, &bottomlevels);

        int j = 0;
        while((j < (igraph_vector_size(&bottomlevels))) && found_low_bottom == 0)
        {
            long int low_bottomlevel_index =  igraph_vector_which_min(&aux_bottomlevels);
            igraph_integer_t b = VECTOR(vertices_top)[low_toplevel_index];
            igraph_integer_t c = VECTOR(vertices_bottom)[low_bottomlevel_index];
	    //            if(have_no_path(graph, c, b) == 1)  //1 == no path
	    if (check_if_path_exists(original_graph->vertices_by_id[c], original_graph->vertices_by_id[b])==0) // no path
            {
                *selected_head_vertex = b;
                *selected_tail_vertex = c;
                found_low_top = 1 ;
                found_low_bottom = 1;
            }
            else
            {
                igraph_real_t max_bottomlevel = igraph_vector_max(&aux_bottomlevels);
                VECTOR(aux_bottomlevels)[low_bottomlevel_index] = max_bottomlevel+1;
            }
            j++;
        }
        igraph_real_t max_toplevel = igraph_vector_max(&toplevels);
        VECTOR(toplevels)[low_toplevel_index] = max_toplevel+1;
        i++;
    }

    igraph_vector_destroy(&toplevels);
    igraph_vector_destroy(&bottomlevels);
    igraph_vector_destroy(&aux_bottomlevels);
    igraph_vector_destroy(&vertices_top);
    igraph_vector_destroy(&vertices_bottom);
    igraph_vector_destroy(&aux_nodework);

    if(found_low_bottom == 0 && found_low_top == 0)
    {
        return 0; //could not find vertices in cut which does not have a path

    }
    return 1; // successfully found two vertices whose edge could be added

}











/*
constructs two vectors (sumWeightsOfposibble_heads and sumWeightsOfposibble_tails) which have the
sum of capacities of the edges for all vertices in the cut;
Example: in sumWieghsOfpossible_heads[9] = 13 where 9 is a vertex in the cut
and the 13 is the sum of weights from the edges were this 9 is the tail.
Sum(k,9)=13 for k in the cut.
*/
int maxsizes(graph_t *original_graph, const igraph_t *graph, igraph_vector_t *cut, igraph_vector_t *rank, igraph_vector_t *nodework,
             igraph_vector_t *capacity, igraph_integer_t source, igraph_integer_t target,
             igraph_integer_t *selected_head_vertex, igraph_integer_t *selected_tail_vertex)
{
    igraph_integer_t a,b,c,d;
    igraph_vector_t sumWeightsOfposibble_heads;
    igraph_vector_t sumWeightsOfposibble_tails;
    igraph_vector_init(&sumWeightsOfposibble_heads, igraph_vcount(graph)); //need to be modified/ may be the nr of vertices
    igraph_vector_init(&sumWeightsOfposibble_tails, igraph_vcount(graph));

    for(int i=0; i < igraph_vector_size(cut); i++)
    {
        igraph_integer_t edge = VECTOR(*cut)[i];
        igraph_edge(graph,edge, &a, &b);
        igraph_real_t edge_capacity = VECTOR(*capacity)[edge];
        VECTOR(sumWeightsOfposibble_heads)[b] += edge_capacity;
        VECTOR(sumWeightsOfposibble_tails)[a] += edge_capacity;
    }
    igraph_vector_t aux_sumWeightsOfpossible_tails;
    igraph_vector_init(&aux_sumWeightsOfpossible_tails, igraph_vector_size(&sumWeightsOfposibble_tails));

    int i =0, found_max_head = 0, found_max_tail = 0;

    igraph_real_t max_head = igraph_vector_max(&sumWeightsOfposibble_heads);

    while(i < igraph_vector_size(&sumWeightsOfposibble_heads) && max_head > 0 && found_max_head == 0)
    {
        long int max_head_index =  igraph_vector_which_max(&sumWeightsOfposibble_heads);
        igraph_vector_update(&aux_sumWeightsOfpossible_tails, &sumWeightsOfposibble_tails);
        int j = 0;
        igraph_real_t max_tail = igraph_vector_max(&aux_sumWeightsOfpossible_tails);
        while(j < igraph_vector_size(&sumWeightsOfposibble_tails) && max_tail > 0 && found_max_tail == 0)
        {
            long int max_tail_index =  igraph_vector_which_max(&aux_sumWeightsOfpossible_tails);
            if(max_head_index != max_tail_index)
            {
	      //if(have_no_path(graph, max_tail_index, max_head_index) == 1)  //1 == no path
	      if (check_if_path_exists(original_graph->vertices_by_id[max_tail_index], original_graph->vertices_by_id[max_head_index])==0) // no path
                {
                    *selected_head_vertex = max_head_index;
                    *selected_tail_vertex = max_tail_index;
                    found_max_head = 1 ;
                    found_max_tail = 1;
                }
                else
                {
                    igraph_integer_t min_tail = igraph_vector_min(&aux_sumWeightsOfpossible_tails);
                    VECTOR(aux_sumWeightsOfpossible_tails)[max_tail_index] = min_tail - 1;
                }
            }
            else
            {
                igraph_integer_t min_tail = igraph_vector_min(&aux_sumWeightsOfpossible_tails);
                VECTOR(aux_sumWeightsOfpossible_tails)[max_tail_index] = min_tail - 1;

            }
            max_tail = igraph_vector_max(&aux_sumWeightsOfpossible_tails);
            j++;
        }
        igraph_integer_t min_head = igraph_vector_min(&sumWeightsOfposibble_heads);
        VECTOR(sumWeightsOfposibble_heads)[max_head_index] = min_head-1;
        max_head = igraph_vector_max(&sumWeightsOfposibble_heads);
        i++;
    }

    igraph_vector_destroy(&aux_sumWeightsOfpossible_tails);
    igraph_vector_destroy(&sumWeightsOfposibble_heads);
    igraph_vector_destroy(&sumWeightsOfposibble_tails);
    if(found_max_head == 0 && found_max_tail == 0)
    {
        return 0; //could not find vertices in cut which does not have a path
    }
    return 1; // successfully found two vertices whose edge could be added


}


igraph_real_t minim(igraph_real_t a, igraph_real_t b)
{
    if(a > b) return b;
    return a;
}
igraph_real_t maxim(igraph_real_t a, igraph_real_t b)
{
    if(a > b) return a;
    return b;
}


// computes two nodes selected_head_vertex and selected_tail_vertex for the edge to be added in restrictGraph
// returns 0 if no such nodes can be found and 1 otherwise

int maxminsize(graph_t *original_graph, const igraph_t *graph, igraph_vector_t *cut, igraph_vector_t *rank, igraph_vector_t *nodework,
               igraph_vector_t *capacity, igraph_integer_t source, igraph_integer_t target,
               igraph_integer_t *selected_head_vertex, igraph_integer_t *selected_tail_vertex)
{
    int size_i = 0, size_j= 0;
    igraph_integer_t u,v;
    igraph_vector_t i_cut_vertices, j_cut_vertices;
    igraph_vector_init(&i_cut_vertices, 0);
    igraph_vector_init(&j_cut_vertices, 0);

    igraph_vector_t sumWeightOfpossible_i, sumWeightOfpossible_j;
    igraph_vector_init(&sumWeightOfpossible_i, igraph_vector_size(&i_cut_vertices));
    igraph_vector_init(&sumWeightOfpossible_j, 0);

    for(int i = 0; i < igraph_vector_size(cut); i++)
    {
        igraph_integer_t edge = VECTOR(*cut)[i];
        igraph_edge(graph,edge, &u, &v);
        igraph_real_t edge_capacity = VECTOR(*capacity)[edge];

        if(!igraph_vector_contains(&j_cut_vertices, u))
        {
            size_j++;
            igraph_vector_resize(&j_cut_vertices, size_j);
            igraph_vector_resize(&sumWeightOfpossible_j, size_j);
            VECTOR(j_cut_vertices)[size_j-1] = u;
            VECTOR(sumWeightOfpossible_j)[size_j-1] = edge_capacity;
        }
        else
        {
            long int pos;
            igraph_vector_search(&j_cut_vertices, 0, u, &pos);
            VECTOR(sumWeightOfpossible_j)[pos] += edge_capacity;
        }
        if(!igraph_vector_contains(&i_cut_vertices, v))
        {
            size_i++;
            igraph_vector_resize(&i_cut_vertices, size_i);
            igraph_vector_resize(&sumWeightOfpossible_i, size_i);
            VECTOR(i_cut_vertices)[size_i-1] = v;
            VECTOR(sumWeightOfpossible_i)[size_i-1] = edge_capacity;
        }
        else
        {
            long int pos;
            igraph_vector_search(&i_cut_vertices, 0, v, &pos);
            VECTOR(sumWeightOfpossible_i)[pos] += edge_capacity;
        }

    }
//summing the cut edge capacities
    int have_found = 0;
    int i = 0, j;
    igraph_real_t minimum_edge_capacity = maxim(igraph_vector_max(&sumWeightOfpossible_i), igraph_vector_max(&sumWeightOfpossible_j));
    igraph_real_t maximum_edge_capacity = minim(igraph_vector_min(&sumWeightOfpossible_i), igraph_vector_min(&sumWeightOfpossible_j));
    while(i < igraph_vector_size(&i_cut_vertices) )
    {
        igraph_integer_t vertex_i = VECTOR(i_cut_vertices)[i];
        j=0;
        while(j < igraph_vector_size(&j_cut_vertices))
        {
            igraph_integer_t vertex_j = VECTOR(j_cut_vertices)[j];
            // igraph_vector_t path_vertices;
            // igraph_vector_init(&path_vertices, 0);
            // igraph_get_shortest_path(graph, &path_vertices, 0, vertex_j, vertex_i, IGRAPH_OUT);
            // if(igraph_vector_size(&path_vertices) == 0)
	    if (check_if_path_exists(original_graph->vertices_by_id[vertex_j], original_graph->vertices_by_id[vertex_i])==0) //no path
            {
                igraph_real_t minim_of_edge_capacities = minim(VECTOR(sumWeightOfpossible_i)[i], VECTOR(sumWeightOfpossible_j)[j]);
                if(maximum_edge_capacity <= minim_of_edge_capacities)
                {
                    maximum_edge_capacity = minim_of_edge_capacities;
                    have_found = 1;
                    *selected_head_vertex = vertex_i;
                    *selected_tail_vertex = vertex_j;
                }
            }
	    // igraph_vector_destroy(&path_vertices);
            j++;
        }
        i++;

    }
    igraph_vector_destroy(&i_cut_vertices);
    igraph_vector_destroy(&j_cut_vertices);
    igraph_vector_destroy(&sumWeightOfpossible_i);
    igraph_vector_destroy(&sumWeightOfpossible_j);
    return have_found;

}

