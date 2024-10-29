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

Processor * tentativeAssignment(vertex_t * v, Processor * pj ,  Cluster* cluster, double &finishTime);

double heuristic(graph_t * graph, Cluster * cluster, int bottomLevelVariant, int evictionVariant, vector<Assignment*> &assignments, double & peakMem);
vector<pair<vertex_t *, double>> calculateBottomLevels(graph_t *graph, int bottomLevelVariant);

double getFinishTimeWithPredecessorsAndBuffers(vertex_t *v, const Processor *pj, const Cluster *cluster, double &startTime);
void correctRtJJsOnPredecessors(Cluster *cluster, const vertex_t *vertexToAssign, const Processor *procToChange);

void kickEdgesThatNeededToKickedToFreeMemForTask( const vector<edge_t*>& edgesToKick, Processor *procToChange);
double removeInputPendingEdgesFromEverywherePendingMemAndBuffer(const Cluster *cluster, const vertex_t *vertexToAssign,
                                                                Processor *procToChange, double availableMem);

double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(const vertex_t *v, const Processor *pj);
double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere3Part(const vertex_t *v, const Processor *pj);

vector<edge_t*> evict( vector<edge_t*> pendingMemories, double &currentlyAvailableBuffer, double &stillTooMuch, bool evictBiggestFirst);

vector<edge_t*> tentativeAssignmentDespiteMemory(vertex_t *v, Processor *pj, Cluster* cluster, double &finishTime, double & startTime, bool &isValid, double &peakMem, bool evictBiigestFirst=false);
void doRealAssignmentWithMemoryAdjustments(Cluster *cluster, double futureReadyTime, const vector<edge_t*> & edgesToKick, vertex_t *vertexToAssign,
                                           Processor *procToChange);
graph_t *convertToNonMemRepresentation(graph_t *withMemories, map<int, int> &noMemToWithMem);

string answerWithJson(vector<Assignment *> assignments, string workflowName);
vector<Assignment*> runAlgorithm(int algorithmNumber, graph_t * graphMemTopology, Cluster *cluster, string workflowName, bool& wasCorrect, double & resultingMS);
vertex_t * getLongestPredecessorWithBuffers(vertex_t *child, const Cluster *cluster, double &latestPredecessorFinishTime);
bool isDelayPossibleUntil(Assignment* assignmentToDelay, double newStartTime, vector<Assignment*> assignments, Cluster* cluster);
std::vector<Assignment*>::iterator findAssignmentByName(vector<Assignment *> &assignments, string name);




#endif //RESHI_TXT_DYNSCHED_HPP
