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

class SchedulingResult {
public:
    shared_ptr<Processor> processorOfAssignment;
    vector<shared_ptr<Processor>> modifiedProcs;
    double finishTime;
    double startTime;
    int resultingVar;
    edge_t *edgeToKick;
    double peakMem;

    explicit SchedulingResult(const shared_ptr<Processor> &proc)
            : processorOfAssignment(proc ? make_shared<Processor>(*proc) : nullptr),
              modifiedProcs{},
              finishTime(0),
              startTime(0),
              resultingVar(-1),
              edgeToKick(nullptr),
              peakMem(0) {}
};

double calculateSimpleBottomUpRank(vertex_t *task);

double calculateBLCBottomUpRank(vertex_t *task);

std::vector<std::pair<vertex_t *, double> > calculateMMBottomUpRank(graph_t *graph);

double new_heuristic(graph_t *graph, Cluster *cluster, int algoNum, bool isHeft);

vector<pair<vertex_t *, double>> calculateBottomLevels(graph_t *graph, int bottomLevelVariant);

double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(const vertex_t *v, const shared_ptr<Processor> &pj);

void tentativeAssignment(vertex_t *v, Cluster *cluster, SchedulingResult &result);

void
tentativeAssignmentHEFT(vertex_t *v, Cluster *cluster, SchedulingResult &result,
                        double &actualFinishTime, double &actualStartTime);

graph_t *convertToNonMemRepresentation(graph_t *withMemories, map<int, int> &noMemToWithMem);

void
processIncomingEdges(const vertex_t *v, shared_ptr<Processor> &ourDesiredProc,
                     vector<std::shared_ptr<Processor>> &modifiedProcs,
                     double &earliestStartingTimeToComputeVertex, Cluster *cluster);

void checkIfPendingMemoryCorrect(const shared_ptr<Processor> &p);

bool hasDuplicates(const std::vector<shared_ptr<Processor>> &vec);

void bestTentativeAssignment(Cluster *cluster, bool isHeft, vertex_t *vertex, SchedulingResult &result);

void realSurplusOfOutgoingEdges(const vertex_t *v, shared_ptr<Processor> &ourModifiedProc, double &sumOut);

#endif //RESHI_TXT_DYNSCHED_HPP
