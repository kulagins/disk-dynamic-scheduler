//
// Created by kulagins on 11.03.24.
//

#ifndef RESHI_TXT_DYNSCHED_DISK_HPP
#define RESHI_TXT_DYNSCHED_DISK_HPP

#include "cluster.hpp"
#include "common.hpp"
#include "graph.hpp"
#include "json.hpp"
#include "sp-graph.hpp"

extern Cluster* cluster;
extern EventManager events;
extern ReadyQueue readyQueue;

double dynMedih(graph_t* graph, Cluster* cluster, int algoNum, int deviationNumber, bool upw);

double applyDeviationTo(double& in);
std::vector<std::shared_ptr<Processor>>
tentativeAssignment(vertex_t* vertex, std::shared_ptr<Processor> ourModifiedProc,
    double& finTime, double& startTime, int& resultingVar, std::vector<std::shared_ptr<Event>>& newEvents,
    double& actuallyUsedMemory, double notEarlierThan = -1);
std::vector<std::shared_ptr<Event>> bestTentativeAssignment(vertex_t* vertex, std::vector<std::shared_ptr<Processor>>& bestModifiedProcs,
    std::shared_ptr<Processor>& bestProcessorToAssign, double notEarlierThan);
std::pair<std::shared_ptr<Event>, std::shared_ptr<Event>> scheduleARead(const vertex_t* v, std::shared_ptr<Event>& ourEvent, std::vector<std::shared_ptr<Event>>& createdEvents, double startTimeOfTask,
    std::shared_ptr<Processor>& ourModifiedProc, edge*& incomingEdge, double& atThisTime);
std::shared_ptr<Processor> findPredecessorsProcessor(edge_t* incomingEdge, std::vector<std::shared_ptr<Processor>>& modifiedProcs);

std::vector<std::shared_ptr<Event>> evictFilesUntilThisFits(std::shared_ptr<Processor> thisProc, edge_t* edgeToFit);
void scheduleWriteAndRead(const vertex_t* v, std::shared_ptr<Event> ourEvent, std::vector<std::shared_ptr<Event>>& createdEvents, double startTimeOfTask,
    std::shared_ptr<Processor>& ourModifiedProc, edge*& incomingEdge, std::vector<std::shared_ptr<Processor>>& modifiedProcs);
double
processIncomingEdges(const vertex_t* v, std::shared_ptr<Event>& ourEvent, std::shared_ptr<Processor>& ourModifiedProc, std::vector<std::shared_ptr<Processor>>& modifiedProcs,
    std::vector<std::shared_ptr<Event>>& createdEvents);

// std::pair<std::shared_ptr<Event>, std::shared_ptr<Event>> scheduleWriteForEdge(std::shared_ptr<Processor> &thisProc, edge_t *edgeToEvict);
std::set<edge_t*, bool (*)(edge_t*, edge_t*)>::iterator
scheduleWriteForEdge(std::shared_ptr<Processor>& thisProc, edge_t* edgeToEvict,
    std::pair<std::shared_ptr<Event>, std::shared_ptr<Event>>& writeEvents, bool onlyPreemptive = false);
void buildPendingMemoriesAfter(std::shared_ptr<Processor>& ourModifiedProc, vertex_t* ourVertex);
void transferAfterMemoriesToBefore(std::shared_ptr<Processor>& ourModifiedProc);
bool dealWithPredecessors(std::shared_ptr<Event> us);
void checkBestEvents(std::vector<std::shared_ptr<Event>>& bestEvents);
double assessWritingOfEdge(edge_t* edge, std::shared_ptr<Processor> proc);

void organizeAReadAndPredecessorWrite(const vertex_t* v, edge* incomingEdge, std::shared_ptr<Event>& ourEvent,
    std::shared_ptr<Processor>& ourModifiedProc, std::vector<std::shared_ptr<Event>>& createdEvents, double afterWhen);
#endif // RESHI_TXT_DYNSCHED_HPP
