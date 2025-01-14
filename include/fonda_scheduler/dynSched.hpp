//
// Created by kulagins on 11.03.24.
//

#ifndef RESHI_TXT_DYNSCHED_HPP
#define RESHI_TXT_DYNSCHED_HPP


#include "graph.hpp"
#include "cluster.hpp"
#include "sp-graph.hpp"
#include "common.hpp"
#include "json.hpp"

double calculateSimpleBottomUpRank(vertex_t *task);
double calculateBLCBottomUpRank(vertex_t *task);
std::vector < std::pair< vertex_t *, double> >  calculateMMBottomUpRank(graph_t * graph);

double new_heuristic(graph_t *graph, Cluster *cluster, int algoNum, bool isHeft);
vector<pair<vertex_t *, double>> calculateBottomLevels(graph_t *graph, int bottomLevelVariant);

double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(const vertex_t *v, const shared_ptr<Processor>&pj);
vector< shared_ptr<Processor>> tentativeAssignment(vertex_t *v, shared_ptr<Processor>pj, double &finishTime,
                                                   double & startTime, double &peakMem, int& resultingVariant, edge* &singleToKick, Cluster * cluster, bool isThisBaseline=false );
vector<shared_ptr<Processor>>
tentativeAssignmentHEFT(vertex_t *v, shared_ptr<Processor> ourModifiedProc,
                        double &actualFinishTime, double &actualStartTime,  double &perceivedFinishTime, double &perceivedStartTime,
                        double &peakMem, Cluster * cluster);

graph_t *convertToNonMemRepresentation(graph_t *withMemories, map<int, int> &noMemToWithMem);
void
processIncomingEdges(const vertex_t *v, shared_ptr<Processor> &ourDesiredProc, vector<std::shared_ptr<Processor>> &modifiedProcs,
                     double &earliestStartingTimeToComputeVertex, Cluster *cluster);
void checkIfPendingMemoryCorrect(const shared_ptr<Processor>& p);
bool hasDuplicates(const std::vector<shared_ptr<Processor>>& vec);
void bestTentativeAssignment(Cluster *cluster, bool isHeft, vertex_t *vertex, vector<shared_ptr<Processor>> &bestModifiedProcs,
                             shared_ptr<Processor> &bestProcessorToAssign, double &bestFinishTime, double &bestStartTime, int &resultingVar,
                             edge_t *&besttoKick, int &numberWithEvictedCases);


#endif //RESHI_TXT_DYNSCHED_HPP
