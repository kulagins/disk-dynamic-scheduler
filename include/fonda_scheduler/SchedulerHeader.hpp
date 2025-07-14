//
// Created by kulagins on 11.03.24.
//

#ifndef RESHI_TXT_DYNSCHED_HPP
#define RESHI_TXT_DYNSCHED_HPP

#include "cluster.hpp"
#include "common.hpp"
#include "graph.hpp"
#include "json.hpp"
#include "sp-graph.hpp"

extern Cluster* imaginedCluster;
extern Cluster* actualCluster;

struct EdgeChange {

    edge_t* edge;
    Location newLocation;

    EdgeChange(edge_t* e, Location nl)
        : edge(e)
        , newLocation(nl)
    {
    }
};

class SchedulingResult {
public:
    std::shared_ptr<Processor> processorOfAssignment;
    std::vector<std::shared_ptr<Processor>> modifiedProcs;
    double finishTime;
    double startTime;
    int resultingVar;
    edge_t* edgeToKick;
    double peakMem;
    std::vector<EdgeChange> edgesToChangeStatus;

    explicit SchedulingResult(const std::shared_ptr<Processor>& proc)
        : processorOfAssignment(proc ? std::make_shared<Processor>(*proc) : nullptr)
        , modifiedProcs {}
        , finishTime(0)
        , startTime(0)
        , resultingVar(-1)
        , edgeToKick(nullptr)
        , peakMem(0)
    {
    }
};

double calculateSimpleBottomUpRank(vertex_t* task);

double calculateBLCBottomUpRank(const vertex_t* task);

std::vector<std::pair<vertex_t*, double>> calculateMMBottomUpRank(graph_t* graphWMems);

double medih(graph_t* graph, int algoNum);

std::vector<std::pair<vertex_t*, double>> calculateBottomLevels(graph_t* graph, int algoNum);

double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(const vertex_t* v, const std::shared_ptr<Processor>& pj);

void tentativeAssignment(const vertex_t* v, bool real, SchedulingResult& result);

void tentativeAssignmentHEFT(const vertex_t* v, bool real, SchedulingResult& result, SchedulingResult& resultCorrect);

graph_t* convertToNonMemRepresentation(graph_t* withMemories, std::map<int, int>& noMemToWithMem);

void processIncomingEdges(const vertex_t* v, bool realAsNotImaginary, bool realAsRealRuntimes, bool isHeft, const std::shared_ptr<Processor>& ourModifiedProc,
    std::vector<std::shared_ptr<Processor>>& modifiedProcs,
    double& earliestStartingTimeToComputeVertex);

void checkIfPendingMemoryCorrect(const std::shared_ptr<Processor>& p);

bool hasDuplicates(const std::vector<std::shared_ptr<Processor>>& vec);

void bestTentativeAssignment(bool isHeft, const vertex_t* vertex, SchedulingResult& result, SchedulingResult& correctResultForHeftOnly);

void realSurplusOfOutgoingEdges(const vertex_t* v, const std::shared_ptr<Processor>& ourModifiedProc, double& sumOut);

void evictAccordingToBestDecision(int& numberWithEvictedCases, SchedulingResult& bestSchedulingResult, const vertex_t* pVertex, bool isHeft, bool real);

void putChangeOnCluster(vertex_t* vertex, SchedulingResult& schedulingResult, Cluster* cluster, int& numberWithEvictedCases, bool real, bool isHeft = false);

std::shared_ptr<Processor> findProcessorThatHoldsEdge(edge_t* incomingEdge, Cluster* clusterToLookIn);

void handleBiggestEvict(bool real, SchedulingResult& result, const std::vector<EdgeChange>& changedEdgesOne,
    double startTimeFor1Evicted, edge_t* biggestPendingEdge, double readyTimeComput);

void handleAllEvict(SchedulingResult& result, double timeToWriteAllPending, const std::vector<EdgeChange>& changedEdgesAll,
    double startTimeForAllEvicted, double readyTimeComput);

#endif // RESHI_TXT_DYNSCHED_HPP
