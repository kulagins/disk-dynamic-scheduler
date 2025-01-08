//
// Created by kulagins on 11.03.24.
//

#ifndef RESHI_TXT_DYNSCHED_DISK_HPP
#define RESHI_TXT_DYNSCHED_DISK_HPP


#include "graph.hpp"
#include "cluster.hpp"
#include "sp-graph.hpp"
#include "common.hpp"
#include "json.hpp"

double new_heuristic_dynamic(graph_t *graph, Cluster *cluster, int algoNum, bool isHeft);
vector<vertex_t*> getReadyTasks(graph_t *graph);
void scheduleReadyTasks();
void playOutExecution(vertex_t* task);


#endif //RESHI_TXT_DYNSCHED_HPP
