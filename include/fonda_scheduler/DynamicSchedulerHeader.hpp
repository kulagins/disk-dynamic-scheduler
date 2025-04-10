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

extern Cluster *cluster;
extern EventManager events;
extern ReadyQueue readyQueue;


double new_heuristic_dynamic(graph_t *graph, Cluster *cluster, int algoNum, bool isHeft, int deviationNumber);

double applyDeviationTo(double &in);
vector<shared_ptr<Processor>>
tentativeAssignment(vertex_t *vertex, shared_ptr<Processor> ourModifiedProc,
double &finTime, double &startTime, int &resultingVar,  vector<shared_ptr<Event>>  &newEvents,
                    double & actuallyUsedMemory, double notEarlierThan=-1);
vector<shared_ptr<Event>>  bestTentativeAssignment( vertex_t *vertex, vector<shared_ptr<Processor>> &bestModifiedProcs,
                             shared_ptr<Processor> &bestProcessorToAssign, double notEarlierThan);
std::pair<shared_ptr<Event>, shared_ptr<Event>> scheduleARead(const vertex_t *v, shared_ptr<Event> &ourEvent, vector<shared_ptr<Event>> &createdEvents, double startTimeOfTask,
                                                              shared_ptr<Processor> &ourModifiedProc, edge *&incomingEdge,  double atThisTime=-1);
shared_ptr<Processor> findPredecessorsProcessor(edge_t * incomingEdge, vector<shared_ptr<Processor>> &modifiedProcs);

vector< shared_ptr<Event>> evictFilesUntilThisFits(shared_ptr<Processor> thisProc, edge_t* edgeToFit);
void scheduleWriteAndRead(const vertex_t *v, shared_ptr<Event>ourEvent, vector<shared_ptr<Event>> &createdEvents, double startTimeOfTask,
                          shared_ptr<Processor> &ourModifiedProc, edge *&incomingEdge, vector<std::shared_ptr<Processor>> &modifiedProcs);
double
processIncomingEdges(const vertex_t *v, shared_ptr<Event> &ourEvent, shared_ptr<Processor> &ourModifiedProc, vector<std::shared_ptr<Processor>> &modifiedProcs,
                    vector<shared_ptr<Event>> &createdEvents);

//std::pair<shared_ptr<Event>, shared_ptr<Event>> scheduleWriteForEdge(shared_ptr<Processor> &thisProc, edge_t *edgeToEvict);
set<edge_t *, bool (*)(edge_t *, edge_t *)>::iterator
scheduleWriteForEdge(shared_ptr<Processor> &thisProc, edge_t *edgeToEvict,
                     std::pair<shared_ptr<Event>, shared_ptr<Event>> &writeEvents);
void buildPendingMemoriesAfter(shared_ptr<Processor> &ourModifiedProc, vertex_t* ourVertex);
void transferAfterMemoriesToBefore(shared_ptr<Processor> &ourModifiedProc);
bool dealWithPredecessors(shared_ptr<Event> us);
void checkBestEvents(vector<shared_ptr<Event>> &bestEvents);
double assessWritingOfEdge(edge_t* edge, shared_ptr<Processor> proc);
#endif //RESHI_TXT_DYNSCHED_HPP
