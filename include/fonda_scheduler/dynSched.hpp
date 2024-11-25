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


 bool heft(graph_t *G, Cluster *cluster, double & makespan, vector<Assignment*> &assignments, double & avgPeakMem);

double calculateSimpleBottomUpRank(vertex_t *task);
double calculateBLCBottomUpRank(vertex_t *task);
std::vector < std::pair< vertex_t *, double> >  calculateMMBottomUpRank(graph_t * graph);

double heuristic(graph_t * graph, Cluster * cluster, int bottomLevelVariant, int evictionVariant, vector<Assignment*> &assignments, double & peakMem);
double new_heuristic(graph_t *graph, Cluster *cluster, bool isHeft);
vector<pair<vertex_t *, double>> calculateBottomLevels(graph_t *graph, int bottomLevelVariant);

double getFinishTimeWithPredecessorsAndBuffers(vertex_t *v, const Processor *pj, const Cluster *cluster, double &startTime);

void kickEdgesThatNeededToKickedToFreeMemForTask( const vector<edge_t*>& edgesToKick, Processor *procToChange);
double removeInputPendingEdgesFromEverywherePendingMemAndBuffer(const Cluster *cluster, const vertex_t *vertexToAssign,
                                                                Processor *procToChange, double availableMem);

double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(const vertex_t *v, const shared_ptr<Processor>pj);

vector<edge_t*> evict( vector<edge_t*> pendingMemories, double &stillTooMuch, bool evictBiggestFirst);

vector< shared_ptr<Processor>> tentativeAssignment(vertex_t *v, shared_ptr<Processor>pj, double &finishTime,
                                                   double & startTime, double &peakMem, int& resultingVariant, edge* &singleToKick, Cluster * cluster, bool isThisBaseline=false );
void doRealAssignmentWithMemoryAdjustments(Cluster *cluster, double futureReadyTime, const vector<edge_t*> & edgesToKick, vertex_t *vertexToAssign,
                                           Processor *procToChange);
graph_t *convertToNonMemRepresentation(graph_t *withMemories, map<int, int> &noMemToWithMem);

string answerWithJson(vector<Assignment *> assignments, string workflowName);
vector<Assignment*> runAlgorithm(int algorithmNumber, graph_t * graphMemTopology, Cluster *cluster, string workflowName, bool& wasCorrect, double & resultingMS);
vertex_t * getLongestPredecessorWithBuffers(vertex_t *child, const Cluster *cluster, double &latestPredecessorFinishTime);
bool isDelayPossibleUntil(Assignment* assignmentToDelay, double newStartTime, vector<Assignment*> assignments, Cluster* cluster);
std::vector<Assignment*>::iterator findAssignmentByName(vector<Assignment *> &assignments, string name);

void
processIncomingEdges(const vertex_t *v, shared_ptr<Processor> &ourModifiedProc, vector<std::shared_ptr<Processor>> &modifiedProcs,
                     double &earliestStartingTimeToComputeVertex, Cluster *cluster);



#endif //RESHI_TXT_DYNSCHED_HPP
