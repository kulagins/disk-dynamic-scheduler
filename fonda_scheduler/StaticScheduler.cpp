#include "fonda_scheduler/SchedulerHeader.hpp"
#include "fonda_scheduler/algorithms.hpp"
#include "fonda_scheduler/options.hpp"

#include <iterator>

Cluster* imaginedCluster;
Cluster* actualCluster;

double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(const vertex_t* v, const std::shared_ptr<Processor>& pj)
{
    assert(!pj->getIsKeptValid() || pj->getAvailableMemory() >= 0);
    double Res = pj->getAvailableMemory() - peakMemoryRequirementOfVertex(v);
    for (int i = 0; i < v->in_degree; i++) {
        const auto inEdge = v->in_edges.at(i);
        if (pj->getPendingMemories().find(inEdge) != pj->getPendingMemories().end()) {
            // incoming edge occupied memory
            Res += inEdge->weight;
        }
    }
    return Res;
}

double medih(graph_t* graph, int algoNum)
{
    const bool isHeft = (algoNum == fonda_scheduler::ALGORITHMS::HEFT);
    if (isHeft) {
        imaginedCluster->mayBecomeInvalid();
    }
    std::vector<std::pair<vertex_t*, double>> ranks = calculateBottomLevels(graph, algoNum);
    removeSourceAndTarget(graph, ranks);
    sort(ranks.begin(), ranks.end(),
        [](const std::pair<vertex_t*, double>& a, const std::pair<vertex_t*, double>& b) {
            return a.second > b.second;
        });
    double makespan = 0, makespanPerceived = 0;
    int numberWithEvictedCases = 0, numberWithEvictedCases2 = 0;
    for (auto& pair : ranks) {
        auto vertex = pair.first;
        // cout<<"deal w "<<vertex->name<<endl;

        SchedulingResult bestSchedulingResult(nullptr);
        SchedulingResult bestSchedulingResultCorrectForHeftOnly(nullptr);
        //  cout << "imagine" << endl;
        bestTentativeAssignment(isHeft, vertex, bestSchedulingResult, bestSchedulingResultCorrectForHeftOnly);

        SchedulingResult bestSchedulingResultOnReal(
            actualCluster->getProcessorById(bestSchedulingResult.processorOfAssignment->id));
        SchedulingResult bestCorrectSchedulingResultOnReal(
            actualCluster->getProcessorById(bestSchedulingResult.processorOfAssignment->id));
        // cout << "real" << endl;
        if (isHeft) {
            tentativeAssignmentHEFT(vertex, true, bestSchedulingResultOnReal, bestCorrectSchedulingResultOnReal);
            bestSchedulingResultOnReal = bestCorrectSchedulingResultOnReal;
        } else {
            bestSchedulingResultOnReal.resultingVar = bestSchedulingResult.resultingVar;
            tentativeAssignment(vertex, true, bestSchedulingResultOnReal);
        }

        if (bestSchedulingResult.modifiedProcs.empty()) {
            std::cout << "Invalid assignment of " << vertex->name;
            return -1;
        } else {
            /* cout  << vertex->name << " best " <<
            " "<< bestSchedulingResult.startTime << " "
                 << bestSchedulingResult.finishTime << " on "
                  << bestSchedulingResult.processorOfAssignment->id
                  << " variant " << bestSchedulingResult.resultingVar
                 <<" with av mem "<<bestSchedulingResult.processorOfAssignment->getAvailableMemory()<<endl;

             cout << " for " << vertex->name << " best real " << bestSchedulingResultOnReal.startTime << " "
                  << bestSchedulingResultOnReal.finishTime << " on proc "
                  << bestSchedulingResultOnReal.processorOfAssignment->id
                  << " variant " << bestSchedulingResultOnReal.resultingVar
                  <<" with av mem "<<bestSchedulingResultOnReal.processorOfAssignment->getAvailableMemory()<<endl; */
        }

        //  cout << "imagine" << endl;
        putChangeOnCluster(vertex, bestSchedulingResult, imaginedCluster, numberWithEvictedCases, false, isHeft);
        // try {
        // cout << "real" << endl;
        putChangeOnCluster(vertex, bestSchedulingResultOnReal, actualCluster, numberWithEvictedCases2, true, isHeft);
        //}
        // catch(...){
        //    cout<<"some error"<<endl;
        //   }

        /*  for (const auto &item: actualCluster->getProcessorById(5)->getPendingMemories()){
              cout<<buildEdgeName(item)<<endl;
          }
          cout<<"<<<<<<<<<"<<endl; */

        if (!isHeft) {
            for (const auto& [proc_id, processor] : imaginedCluster->getProcessors()) {
                if (processor->getPendingMemories().size() != actualCluster->getProcessorById(processor->id)->getPendingMemories().size()) {
                    for (const auto& item2 : processor->getPendingMemories()) {
                        std::cout << buildEdgeName(item2) << '\n';
                    }
                    std::cout << "----------------" << '\n';
                    for (const auto& item2 : actualCluster->getProcessorById(processor->id)->getPendingMemories()) {
                        std::cout << buildEdgeName(item2) << '\n';
                    }
                    assert(processor->getPendingMemories().size() == actualCluster->getProcessorById(processor->id)->getPendingMemories().size());
                }
            }
        }

        vertex->makespanPerceived = bestSchedulingResult.finishTime;
        assert(bestSchedulingResult.startTime < bestSchedulingResult.finishTime);

        vertex->makespan = bestSchedulingResultOnReal.finishTime;

        if (makespan < bestSchedulingResultOnReal.finishTime)
            makespan = bestSchedulingResultOnReal.finishTime;
        if (makespanPerceived < bestSchedulingResult.finishTime)
            makespanPerceived = bestSchedulingResult.finishTime;
    }
    std::cout << // " #eviction " << numberWithEvictedCases << " " <<
        " ms perceived " << makespanPerceived << " ";
    return makespan;
}

void bestTentativeAssignment(const bool isHeft, vertex_t* vertex, SchedulingResult& result,
    SchedulingResult& correctResultForHeftOnly)
{
    result.finishTime = std::numeric_limits<double>::max();
    result.startTime = 0;

    for (auto& [id, processor] : imaginedCluster->getProcessors()) {
        //  cout << "try proc " << processor->id << endl;
        SchedulingResult tentativeResult(processor);
        SchedulingResult correctTentativeResultForHeftOnly(actualCluster->getProcessorById(processor->id));

        checkIfPendingMemoryCorrect(processor);
        if (isHeft) {
            tentativeAssignmentHEFT(vertex, false, tentativeResult, correctTentativeResultForHeftOnly);
        } else {
            tentativeAssignment(vertex, false, tentativeResult);
        }
        //  cout<<"tentative ft on "<<processor->id<<" is "<<tentativeResult.finishTime <<(tentativeResult.resultingVar>1? "overflow!":"")<<endl;

        if (!isHeft) {
            checkIfPendingMemoryCorrect(tentativeResult.processorOfAssignment);
        }
        // cout<<"start "<<startTime<<" end "<<finTime<<endl;
        // if (tentativeResult.startTime != 0) {
        //     cout<<"not actual "<<finTime<<" vs "<<actualFinishTime<<" on "<<processor->id<<endl;
        // }

        if (tentativeResult.finishTime < result.finishTime
            || (result.finishTime == tentativeResult.finishTime && result.processorOfAssignment && tentativeResult.processorOfAssignment->getMemorySize() > result.processorOfAssignment->getMemorySize())) {
            assert(!tentativeResult.modifiedProcs.empty());
            result = tentativeResult;
            if (isHeft) {
                correctResultForHeftOnly = correctTentativeResultForHeftOnly;
                // if (result.startTime != correctTentativeResultForHeftOnly.startTime) {
                //     resultnumberWithEvictedCases++;
                //     throw runtime_error("numberWithEvictedCases++;");
                //      cout << "increase numWithEvictged in HEFT" << endl;
                // }
                result.resultingVar = 1;
            }
        }
    }
}

void tentativeAssignment(vertex_t* v, const bool real, SchedulingResult& result)
{

    const double timeToRun = real ? v->time * v->factorForRealExecution : v->time;

    double sumOut = getSumOut(v);

    assert(sumOut == outMemoryRequirement(v));
    if (result.processorOfAssignment->getMemorySize() < outMemoryRequirement(v) || result.processorOfAssignment->getMemorySize() < inMemoryRequirement(v)) {
        //  cout<<"too large outs absolutely"<<endl;
        result.finishTime = std::numeric_limits<double>::max();
        return;
    }
    realSurplusOfOutgoingEdges(v, result.processorOfAssignment, sumOut);

    std::vector<std::shared_ptr<Processor>> modifiedProcs;
    modifiedProcs.emplace_back(result.processorOfAssignment);
    processIncomingEdges(v, true, real, false, result.processorOfAssignment, modifiedProcs, result.startTime);

    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    result.startTime = result.processorOfAssignment->getReadyTimeCompute() > result.startTime
        ? result.processorOfAssignment->getReadyTimeCompute()
        : result.startTime;

    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    checkIfPendingMemoryCorrect(result.processorOfAssignment);

    const double Res = howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(v, result.processorOfAssignment);
    result.peakMem = (Res < 0) ? 1 : (result.processorOfAssignment->getMemorySize() - Res) / result.processorOfAssignment->getMemorySize();

    if (Res < 0) {
        // try finish times with and without memory overflow
        const double amountToOffload = -Res;

        double timeToFinishNoEvicted = result.startTime + timeToRun / result.processorOfAssignment->getProcessorSpeed() + amountToOffload / result.processorOfAssignment->memoryOffloadingPenalty;
        assert(timeToFinishNoEvicted > result.startTime);
        if (sumOut > result.processorOfAssignment->getAvailableMemory()) {
            // cout<<"cant"<<endl;
            timeToFinishNoEvicted = std::numeric_limits<double>::max();
        }

        double timeToFinishBiggestEvicted = std::numeric_limits<double>::max(),
               timeToFinishAllEvicted = std::numeric_limits<double>::max();
        double timeToWriteAllPending = 0;
        std::vector<EdgeChange> changedEdgesOne, changedEdgesAll;

        double startTimeFor1Evicted = result.processorOfAssignment->getReadyTimeWrite() > result.startTime ? result.processorOfAssignment->getReadyTimeWrite() : result.startTime;
        double startTimeForAllEvicted = startTimeFor1Evicted;

        auto biggestPendingEdge = result.processorOfAssignment->getBiggestPendingEdgeThatIsNotIncomingOfAndLocatedOnProc(
            v);
        if (!result.processorOfAssignment->getPendingMemories().empty() && biggestPendingEdge != nullptr) {
            assert((*result.processorOfAssignment->getPendingMemories().begin())->weight >= (*result.processorOfAssignment->getPendingMemories().rbegin())->weight);

            const auto biggestFileWeight = biggestPendingEdge->weight;
            const double amountToOffloadWithoutBiggestFile = (amountToOffload - biggestFileWeight) > 0 ? (amountToOffload - biggestFileWeight) : 0;
            const double biggestWeightToWrite = real ? biggestPendingEdge->weight * biggestPendingEdge->factorForRealExecution : biggestPendingEdge->weight;
            const double startTimeToWriteBiggestEdge = std::max(result.processorOfAssignment->getReadyTimeWrite(),
                real ? biggestPendingEdge->tail->makespan : biggestPendingEdge->tail->makespanPerceived);
            double finishTimeToWrite = startTimeToWriteBiggestEdge + biggestWeightToWrite / result.processorOfAssignment->writeSpeedDisk;
            changedEdgesOne.emplace_back(biggestPendingEdge, Location(LocationType::OnDisk, std::nullopt, finishTimeToWrite));

            startTimeFor1Evicted = std::max(result.startTime, finishTimeToWrite);
            timeToFinishBiggestEvicted = startTimeFor1Evicted
                + timeToRun / result.processorOfAssignment->getProcessorSpeed() + amountToOffloadWithoutBiggestFile / result.processorOfAssignment->memoryOffloadingPenalty;
            assert(timeToFinishBiggestEvicted > startTimeFor1Evicted);

            const double availableMemWithoutBiggest = result.processorOfAssignment->getAvailableMemory() + biggestFileWeight;
            if (sumOut > availableMemWithoutBiggest)
                timeToFinishBiggestEvicted = std::numeric_limits<double>::max();

            double sumWeightsOfAllPending = 0;
            finishTimeToWrite = result.processorOfAssignment->getReadyTimeWrite();
            for (const auto& item : result.processorOfAssignment->getPendingMemories()) {
                if (item->head->name != v->name) {
                    const double startTimeWrite = std::max(finishTimeToWrite,
                        (real ? item->tail->makespan : item->tail->makespanPerceived));
                    const double itemWeightToWrite = real ? item->weight * item->factorForRealExecution : item->weight;
                    timeToWriteAllPending += itemWeightToWrite / result.processorOfAssignment->writeSpeedDisk;
                    finishTimeToWrite = startTimeWrite + itemWeightToWrite / result.processorOfAssignment->writeSpeedDisk;
                    changedEdgesAll.emplace_back(item, Location(LocationType::OnDisk, std::nullopt, finishTimeToWrite));
                    sumWeightsOfAllPending += item->weight;
                }
            }

            const double amountToOffloadWithoutAllFiles = (amountToOffload - sumWeightsOfAllPending > 0) ? amountToOffload - sumWeightsOfAllPending : 0;

            assert(amountToOffloadWithoutAllFiles >= 0);
            //  finishTimeToWrite = result.processorOfAssignment->getReadyTimeWrite() +
            //                     timeToWriteAllPending;
            startTimeForAllEvicted = std::max(startTimeForAllEvicted, finishTimeToWrite);
            timeToFinishAllEvicted = startTimeForAllEvicted + timeToRun / result.processorOfAssignment->getProcessorSpeed() + amountToOffloadWithoutAllFiles / result.processorOfAssignment->memoryOffloadingPenalty;
            assert(timeToFinishAllEvicted > startTimeForAllEvicted);
        }

        const double minTTF = std::min(timeToFinishNoEvicted, std::min(timeToFinishBiggestEvicted, timeToFinishAllEvicted));
        if (minTTF == std::numeric_limits<double>::max()) {
            std::cout << "minTTF inf" << '\n';
            result.finishTime = std::numeric_limits<double>::max();
            return;
        }

        if (result.resultingVar != -1) {
            assert(real);

            if (result.resultingVar == 2) {
                handleBiggestEvict(real, result, changedEdgesOne, startTimeFor1Evicted, biggestPendingEdge, timeToFinishBiggestEvicted);
            } else if (result.resultingVar == 3) {
                handleAllEvict(result, timeToWriteAllPending, changedEdgesAll, startTimeForAllEvicted, timeToFinishAllEvicted);
                // assert(minTTF==timeToFinishAllEvicted);
            } else {
                //  assert(minTTF==timeToFinishNoEvicted);
                assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
                result.processorOfAssignment->setReadyTimeCompute(timeToFinishNoEvicted);
                result.finishTime = result.processorOfAssignment->getReadyTimeCompute();
                assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
            }
        } else {
            if (timeToFinishBiggestEvicted == minTTF) {
                handleBiggestEvict(real, result, changedEdgesOne, startTimeFor1Evicted, biggestPendingEdge, minTTF);
            } else if (timeToFinishAllEvicted == minTTF) {
                handleAllEvict(result, timeToWriteAllPending, changedEdgesAll, startTimeForAllEvicted, minTTF);
            } else {
                result.resultingVar = 1;
                assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
                result.processorOfAssignment->setReadyTimeCompute(minTTF);
                result.finishTime = result.processorOfAssignment->getReadyTimeCompute();
                assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
            }
        }

    } else {
        // startTime =  ourModifiedProc->readyTimeCompute;
        //  printInlineDebug("should be successful");
        result.resultingVar = 1;
        result.processorOfAssignment->setReadyTimeCompute(
            result.startTime + timeToRun / result.processorOfAssignment->getProcessorSpeed());
        result.finishTime = result.processorOfAssignment->getReadyTimeCompute();
    }
    // cout<<endl;
    assert(result.finishTime > result.startTime);
    result.modifiedProcs = modifiedProcs;
    assert(result.resultingVar != -1);
}

void tentativeAssignmentHEFT(vertex_t* v, const bool real, SchedulingResult& result, SchedulingResult& resultCorrect)
{
    // cout<<"tent on proc "<<ourModifiedProc->id<< " ";
    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());

    const double timeToRun = real ? v->time * v->factorForRealExecution : v->time;

    double sumOut = getSumOut(v);
    assert(sumOut == outMemoryRequirement(v));
    if (result.processorOfAssignment->getMemorySize() < outMemoryRequirement(v) || result.processorOfAssignment->getMemorySize() < inMemoryRequirement(v)) {
        //  cout<<"too large outs absolutely"<<endl;
        result.finishTime = std::numeric_limits<double>::max();
        return;
    }

    // cout<<"sumOut includes ";
    realSurplusOfOutgoingEdges(v, resultCorrect.processorOfAssignment, sumOut);

    std::vector<std::shared_ptr<Processor>> modifiedProcs, modifiedProcsCorrect;
    modifiedProcs.emplace_back(result.processorOfAssignment);
    modifiedProcsCorrect.emplace_back(resultCorrect.processorOfAssignment);

    processIncomingEdges(v, false, false, true, result.processorOfAssignment, modifiedProcs, result.startTime);
    processIncomingEdges(v, true, real, true, resultCorrect.processorOfAssignment, modifiedProcsCorrect,
        resultCorrect.startTime);

    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    // both processIncomingEdges do the same, so start times will be the same
    result.startTime = result.processorOfAssignment->getReadyTimeCompute() > result.startTime ? result.processorOfAssignment->getReadyTimeCompute() : result.startTime;

    resultCorrect.startTime = resultCorrect.processorOfAssignment->getReadyTimeCompute() > resultCorrect.startTime ? resultCorrect.processorOfAssignment->getReadyTimeCompute() : resultCorrect.startTime;

    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());

    if (resultCorrect.processorOfAssignment->getAvailableMemory() < sumOut) {
        // only the correct result knows about kicking

        double stillNeedsToBeEvictedToRun = sumOut - resultCorrect.processorOfAssignment->getAvailableMemory();
        double writeTime = resultCorrect.startTime > resultCorrect.processorOfAssignment->getReadyTimeCompute()
            ? resultCorrect.startTime
            : result.processorOfAssignment->getReadyTimeCompute();

        for (auto it = resultCorrect.processorOfAssignment->getPendingMemories().begin();
            it != resultCorrect.processorOfAssignment->getPendingMemories().end() && stillNeedsToBeEvictedToRun > 0;) {
            //  print_edge(*it);
            if ((*it)->head->name != v->name) {
                const double weightForTime = real ? (*it)->weight * (*it)->factorForRealExecution : (*it)->weight;
                stillNeedsToBeEvictedToRun -= (*it)->weight;
                const double startWriteTime = std::max(writeTime, real ? (*it)->tail->makespan : (*it)->tail->makespanPerceived);
                writeTime = startWriteTime + weightForTime / resultCorrect.processorOfAssignment->writeSpeedDisk;
                //   cout<<"tent on proc "<<resultCorrect.processorOfAssignment->id<<" ";
                resultCorrect.edgesToChangeStatus.emplace_back((*it), Location(LocationType::OnDisk, std::nullopt, writeTime));
                it = resultCorrect.processorOfAssignment->removePendingMemory(*it);

            } else {
                ++it;
            }
        }
        if (stillNeedsToBeEvictedToRun > 0) {
            std::cout << buildEdgeName(*resultCorrect.processorOfAssignment->getPendingMemories().begin()) << '\n';
            throw std::runtime_error("stillNeedsToBeEvictedToRun > 0");
        }
        assert(stillNeedsToBeEvictedToRun <= 0);
        assert(resultCorrect.processorOfAssignment->getAvailableMemory() >= sumOut);
        resultCorrect.startTime = writeTime;
        resultCorrect.processorOfAssignment->setReadyTimeWrite(writeTime);
        resultCorrect.processorOfAssignment->setReadyTimeCompute(writeTime);
        assert(resultCorrect.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    }

    const double Res = howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(v, resultCorrect.processorOfAssignment);
    result.peakMem = (Res < 0) ? 1 : (result.processorOfAssignment->getMemorySize() - Res) / result.processorOfAssignment->getMemorySize();

    result.finishTime = result.startTime + v->time / result.processorOfAssignment->getProcessorSpeed();
    result.processorOfAssignment->setReadyTimeCompute(result.finishTime);

    if (Res < 0) {
        // try finish times with and without memory overflow
        const double amountToOffload = -Res;

        resultCorrect.finishTime = resultCorrect.startTime + timeToRun / resultCorrect.processorOfAssignment->getProcessorSpeed() + amountToOffload / resultCorrect.processorOfAssignment->memoryOffloadingPenalty;
        assert(resultCorrect.finishTime > resultCorrect.startTime);

        if (result.finishTime == std::numeric_limits<double>::max()) {
            std::cout << "perceivedFinishTime inf" << '\n';
            resultCorrect.finishTime = std::numeric_limits<double>::max();
            return;
        }
        assert(resultCorrect.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
        resultCorrect.processorOfAssignment->setReadyTimeCompute(resultCorrect.finishTime);
        assert(resultCorrect.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());

    } else {
        resultCorrect.finishTime = resultCorrect.startTime + timeToRun / resultCorrect.processorOfAssignment->getProcessorSpeed();
        resultCorrect.processorOfAssignment->setReadyTimeCompute(resultCorrect.finishTime);
    }
    result.modifiedProcs = modifiedProcs;
    resultCorrect.modifiedProcs = modifiedProcsCorrect;
    result.resultingVar = 1;
    resultCorrect.resultingVar = 1;
}

void evictAccordingToBestDecision(int& numberWithEvictedCases, SchedulingResult& bestSchedulingResult, const vertex_t* pVertex,
    const bool isHeft,
    const bool real)
{
    const bool shouldUseImaginary = isHeft && !real;
    const bool canAlreadyBeEvicted = !isHeft && real;
    edge_t* edgeToKick = bestSchedulingResult.edgeToKick;
    switch (bestSchedulingResult.resultingVar) {
    case 1:
        break;
    case 2: {
        //    cout<<"best with 1 kick"<<endl;
        assert(edgeToKick != nullptr);
        assert(bestSchedulingResult.edgesToChangeStatus.size() == 1);

        const auto findEdgeInChanges = std::find_if(
            bestSchedulingResult.edgesToChangeStatus.begin(),
            bestSchedulingResult.edgesToChangeStatus.end(), [edgeToKick](const EdgeChange& e) {
                return edgeToKick == e.edge;
            });
        assert(findEdgeInChanges != bestSchedulingResult.edgesToChangeStatus.end());

        canAlreadyBeEvicted ? bestSchedulingResult.processorOfAssignment->delocateToDiskOptionally(edgeToKick,
                                  shouldUseImaginary, findEdgeInChanges->newLocation.afterWhen.value())
                            : bestSchedulingResult.processorOfAssignment->delocateToDisk(
                                  edgeToKick,
                                  shouldUseImaginary, findEdgeInChanges->newLocation.afterWhen.value());
        numberWithEvictedCases++;
        checkIfPendingMemoryCorrect(bestSchedulingResult.processorOfAssignment);
        break;
    }
    case 3:
        // cout<<"best with all kick"<<endl;
        assert(bestSchedulingResult.edgesToChangeStatus.size() > 1);
        for (auto it = bestSchedulingResult.processorOfAssignment->getPendingMemories().begin();
            it != bestSchedulingResult.processorOfAssignment->getPendingMemories().end();) {
            edge_t* nextEdge = *it;
            // cout << buildEdgeName(nextEdge) << endl;
            if (nextEdge->head->name != pVertex->name) {

                const auto findEdgeInChanges1 = std::find_if(
                    bestSchedulingResult.edgesToChangeStatus.begin(),
                    bestSchedulingResult.edgesToChangeStatus.end(), [nextEdge](const EdgeChange& e) {
                        return nextEdge == e.edge;
                    });
                assert(findEdgeInChanges1 != bestSchedulingResult.edgesToChangeStatus.end());

                it = canAlreadyBeEvicted ? bestSchedulingResult.processorOfAssignment->delocateToDiskOptionally(nextEdge,
                                               shouldUseImaginary,
                                               findEdgeInChanges1->newLocation.afterWhen.value())
                                         : bestSchedulingResult.processorOfAssignment->delocateToDisk(nextEdge,
                                               shouldUseImaginary,
                                               findEdgeInChanges1->newLocation.afterWhen.value());
                assert(isLocatedOnDisk(nextEdge, shouldUseImaginary));
            } else {
                ++it;
            }
        }
        assert(bestSchedulingResult.processorOfAssignment->getPendingMemories().empty()
            || (*bestSchedulingResult.processorOfAssignment->getPendingMemories().begin())->head->name == pVertex->name);
        numberWithEvictedCases++;
        checkIfPendingMemoryCorrect(bestSchedulingResult.processorOfAssignment);
        break;
    default:
        throw std::runtime_error("");
    }
}

void putChangeOnCluster(vertex_t* vertex, SchedulingResult& schedulingResult, Cluster* cluster, int& numberWithEvictedCases,
    const bool real, const bool isHeft)
{

    const bool shouldUseImaginary = isHeft && !real;
    evictAccordingToBestDecision(numberWithEvictedCases, schedulingResult, vertex, isHeft, real);

    for (auto& modifiedProc : schedulingResult.modifiedProcs) {
        checkIfPendingMemoryCorrect(modifiedProc);
        const auto procInClusterWithId = cluster->getProcessorById(modifiedProc->id);
        procInClusterWithId->updateFrom(*modifiedProc);
    }

    for (auto e : schedulingResult.edgesToChangeStatus) {
        //  cout<<"change status "<<buildEdgeName(e.edge)<<endl;
        assert(!shouldUseImaginary);
        if (isLocatedOnThisProcessor(e.edge, schedulingResult.processorOfAssignment->id, shouldUseImaginary)) {
            delocateFromThisProcessorToDisk(e.edge, schedulingResult.processorOfAssignment->id, shouldUseImaginary,
                e.newLocation.afterWhen.value());
        }
    }

    assert(schedulingResult.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    vertex->assignedProcessorId = schedulingResult.processorOfAssignment->id;

    for (int j = 0; j < vertex->in_degree; j++) {
        edge_t* ine = vertex->in_edges[j];

        const int onWhichProcessor = whatProcessorIsLocatedOn(ine, shouldUseImaginary);
        assert(onWhichProcessor == -1 || onWhichProcessor == schedulingResult.processorOfAssignment->id || cluster->getProcessorById(onWhichProcessor)->getPendingMemories().find(ine) == cluster->getProcessorById(onWhichProcessor)->getPendingMemories().end());

        if (onWhichProcessor == schedulingResult.processorOfAssignment->id) {
            // optionally, because edge could have been force removed during calculation of caorrect result in HEFT
            schedulingResult.processorOfAssignment->delocateToNowhereOptionally(ine, shouldUseImaginary, -1);
        } else {
            if (onWhichProcessor != -1) {
                cluster->getProcessorById(onWhichProcessor)->delocateToNowhereOptionally(ine, shouldUseImaginary, -1);
            } else {
                // edge has been read
                // cout<<"bla"<<endl;
                // cout << "NOWHERE! " << buildEdgeName(ine) << endl;
                const auto proc = findProcessorThatHoldsEdge(ine, cluster);
                if (proc != nullptr) {
                    if (proc->id == schedulingResult.processorOfAssignment->id)
                        schedulingResult.processorOfAssignment->delocateToNowhereOptionally(ine, shouldUseImaginary, -1);
                    else
                        proc->delocateToNowhereOptionally(ine, shouldUseImaginary, -1);
                }

                // assert(proc == nullptr);
            }
        }
        if (shouldUseImaginary)
            ine->imaginedLocations.clear();
        else
            ine->locations.clear();
    }

    checkIfPendingMemoryCorrect(schedulingResult.processorOfAssignment);

    for (int i = 0; i < vertex->out_degree; i++) {
        const auto v1 = vertex->out_edges[i];
        schedulingResult.processorOfAssignment->loadFromNowhere(v1, shouldUseImaginary, schedulingResult.finishTime);
        checkIfPendingMemoryCorrect(schedulingResult.processorOfAssignment);
        if (schedulingResult.processorOfAssignment->getAvailableMemory() < 0) {
            std::cout << "";
        }
    }
    cluster->getProcessorById(schedulingResult.processorOfAssignment->id)->updateFrom(*schedulingResult.processorOfAssignment);
    for (const auto& [proc_id, processor] : cluster->getProcessors()) {
        checkIfPendingMemoryCorrect(processor);
    }
}

void realSurplusOfOutgoingEdges(const vertex_t* v, const std::shared_ptr<Processor>& ourModifiedProc, double& sumOut)
{
    for (int i = 0; i < v->in_degree; i++) {
        auto inEdge = v->in_edges.at(i);
        if (isLocatedOnThisProcessor(inEdge, ourModifiedProc->id, false)) {
            //     cout<<"in is located here "; print_edge(v->in_edges[i]);
            auto pendingOfProc = ourModifiedProc->getPendingMemories();
            // assert(pendingOfProc.find(inEdge) != pendingOfProc.end());
            if (pendingOfProc.find(inEdge) != pendingOfProc.end()) {
                sumOut -= inEdge->weight;
            } else {
                // cout<<"edge "<<buildEdgeName(inEdge)<<" not anymore found in pending mems of processor "<<ourModifiedProc->id<<endl;
            }
        }
    }
    //  cout << "REQUIRES AT THE END: " << sumOut << endl;
}

void processIncomingEdges(const vertex_t* v, const bool realAsNotImaginary, const bool realAsRealRuntimes, const bool isHeft,
    const std::shared_ptr<Processor>& ourModifiedProc,
    std::vector<std::shared_ptr<Processor>>& modifiedProcs,
    double& earliestStartingTimeToComputeVertex)
{

    const bool shouldUseImaginary = isHeft & !realAsNotImaginary;
    earliestStartingTimeToComputeVertex = ourModifiedProc->getReadyTimeCompute();
    for (int j = 0; j < v->in_degree; j++) {
        edge_t* incomingEdge = v->in_edges.at(j);
        const vertex_t* predecessor = incomingEdge->tail;

        const double edgeWeightToUse = realAsRealRuntimes ? incomingEdge->weight * incomingEdge->factorForRealExecution
                                                          : incomingEdge->weight;

        if (predecessor->assignedProcessorId == ourModifiedProc->id) {
            if (!isLocatedOnThisProcessor(incomingEdge, ourModifiedProc->id, shouldUseImaginary)) {
                assert(isLocatedOnDisk(incomingEdge, shouldUseImaginary));
                ourModifiedProc->setReadyTimeRead(
                    ourModifiedProc->getReadyTimeRead() + edgeWeightToUse / ourModifiedProc->readSpeedDisk);
                earliestStartingTimeToComputeVertex = ourModifiedProc->getReadyTimeRead() > earliestStartingTimeToComputeVertex ? ourModifiedProc->getReadyTimeRead() : earliestStartingTimeToComputeVertex;
            }

        } else {
            if (isLocatedOnDisk(incomingEdge, shouldUseImaginary)) {
                // we need to schedule read
                const double startOfRead = std::max(ourModifiedProc->getReadyTimeRead(), getLocationOnDisk(incomingEdge, shouldUseImaginary).afterWhen.value());
                ourModifiedProc->setReadyTimeRead(
                    startOfRead + edgeWeightToUse / ourModifiedProc->readSpeedDisk);
                earliestStartingTimeToComputeVertex = ourModifiedProc->getReadyTimeRead() > earliestStartingTimeToComputeVertex ? ourModifiedProc->getReadyTimeRead() : earliestStartingTimeToComputeVertex;

            } else {
                auto predecessorsProcessorsId = predecessor->assignedProcessorId;
                assert(isLocatedOnThisProcessor(incomingEdge, predecessorsProcessorsId, shouldUseImaginary));
                std::shared_ptr<Processor> addedProc;
                auto it = // modifiedProcs.size()==1?
                          //   modifiedProcs.begin():
                    std::find_if(modifiedProcs.begin(), modifiedProcs.end(),
                        [predecessorsProcessorsId](const std::shared_ptr<Processor>& p) {
                            return p->id == predecessorsProcessorsId;
                        });

                if (it == modifiedProcs.end()) {
                    Cluster* cluster = realAsNotImaginary ? actualCluster : imaginedCluster;
                    addedProc = std::make_shared<Processor>(*cluster->getProcessorById(predecessorsProcessorsId));
                    // cout<<"adding modified proc "<<addedProc->id<<endl;
                    modifiedProcs.emplace_back(addedProc);
                    checkIfPendingMemoryCorrect(addedProc);
                } else {
                    addedProc = *it;
                }

                assert(!hasDuplicates(modifiedProcs));

                double whichMakespan = realAsRealRuntimes ? predecessor->makespan : predecessor->makespanPerceived;
                const double timeToStartWriting = std::max(whichMakespan, addedProc->getReadyTimeWrite());
                addedProc->setReadyTimeWrite(timeToStartWriting + edgeWeightToUse / addedProc->writeSpeedDisk);
                const double startTimeOfRead = std::max(addedProc->getReadyTimeWrite(), ourModifiedProc->getReadyTimeRead());

                double endTimeOfRead = startTimeOfRead + edgeWeightToUse / ourModifiedProc->readSpeedDisk;
                ourModifiedProc->setReadyTimeRead(endTimeOfRead);

                earliestStartingTimeToComputeVertex = std::max(earliestStartingTimeToComputeVertex, endTimeOfRead);
                // int addpl  = addedProc->pendingMemories.size();
                addedProc->removePendingMemory(incomingEdge);
                // assert(addpl> addedProc->pendingMemories.size());
                checkIfPendingMemoryCorrect(addedProc);
            }
        }
    }
}

void handleBiggestEvict(const bool real, SchedulingResult& result, const std::vector<EdgeChange>& changedEdgesOne,
    const double startTimeFor1Evicted, edge_t* biggestPendingEdge, const double readyTimeComput)
{
    assert(biggestPendingEdge != nullptr);
    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    result.processorOfAssignment->setReadyTimeCompute(readyTimeComput);
    result.finishTime = result.processorOfAssignment->getReadyTimeCompute();
    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());

    result.edgeToKick = biggestPendingEdge;
    // cout<<"best tentative with biggest Evicted "; print_edge(toKick);
    result.resultingVar = 2;
    const double biggestWeightToWrite = real ? biggestPendingEdge->weight * biggestPendingEdge->factorForRealExecution
                                             : biggestPendingEdge->weight;
    result.processorOfAssignment->setReadyTimeWrite(result.processorOfAssignment->getReadyTimeWrite() + biggestWeightToWrite / result.processorOfAssignment->writeSpeedDisk);
    // ourModifiedProc->pendingMemories.erase()
    // penMemsAsVector.erase(penMemsAsVector.begin());
    result.edgesToChangeStatus = changedEdgesOne;
    assert(result.startTime <= startTimeFor1Evicted);
    result.startTime = startTimeFor1Evicted;
    assert(result.edgeToKick != nullptr);
    assert(!result.edgeToKick->locations.empty());
    assert(isLocatedOnThisProcessor(result.edgeToKick, result.processorOfAssignment->id, false));
}

void handleAllEvict(SchedulingResult& result, const double timeToWriteAllPending, const std::vector<EdgeChange>& changedEdgesAll,
    const double startTimeForAllEvicted, const double readyTimeComput)
{

    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    result.processorOfAssignment->setReadyTimeCompute(readyTimeComput);
    result.finishTime = result.processorOfAssignment->getReadyTimeCompute();
    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    result.resultingVar = 3;
    // cout<<"best tentative with all Evicted ";
    result.processorOfAssignment->setReadyTimeWrite(
        result.processorOfAssignment->getReadyTimeWrite() + timeToWriteAllPending);
    assert(result.startTime <= startTimeForAllEvicted);
    result.startTime = startTimeForAllEvicted;
    result.edgesToChangeStatus = changedEdgesAll;
    // penMemsAsVector.resize(0);
}

graph_t* convertToNonMemRepresentation(graph_t* withMemories, std::map<int, int>& noMemToWithMem)
{
    enforce_single_source_and_target(withMemories);
    graph_t* noNodeMemories = new_graph();

    for (vertex_t* vertex = withMemories->source; vertex; vertex = next_vertex_in_sorted_topological_order(withMemories,
                                                              vertex,
                                                              &sort_by_increasing_top_level)) {
        vertex_t* invtx = new_vertex(noNodeMemories, vertex->name + "-in", vertex->time, nullptr);
        noMemToWithMem.insert({ invtx->id, vertex->id });
        if (!noNodeMemories->source) {
            noNodeMemories->source = invtx;
        }
        vertex_t* outvtx = new_vertex(noNodeMemories, vertex->name + "-out", 0.0, nullptr);
        noMemToWithMem.insert({ outvtx->id, vertex->id });
        edge_t* e = new_edge(noNodeMemories, invtx, outvtx, vertex->memoryRequirement, nullptr);
        noNodeMemories->target = outvtx;

        for (int i = 0; i < vertex->in_degree; i++) {
            const edge_t* inEdgeOriginal = vertex->in_edges[i];
            const std::string expectedName = inEdgeOriginal->tail->name + "-out";
            vertex_t* outVtxOfCopiedInVtxOfEdge = findVertexByName(noNodeMemories, expectedName);

            if (outVtxOfCopiedInVtxOfEdge == nullptr) {
                print_graph_to_cout(noNodeMemories);
                outVtxOfCopiedInVtxOfEdge = findVertexByName(noNodeMemories, expectedName);
                std::cout << "expected: " << expectedName << '\n';
                throw std::invalid_argument(" no vertex found for expected name.");
            }
            edge_t* e_new = new_edge(noNodeMemories, outVtxOfCopiedInVtxOfEdge, invtx, inEdgeOriginal->weight, nullptr);
        }
    }

    return noNodeMemories;
}

double calculateSimpleBottomUpRank(vertex_t* task)
{
    //    cout<<"rank for "<<task->name<<" ";

    double maxCost = 0.0;
    for (int j = 0; j < task->out_degree; j++) {
        const double communicationCost = task->out_edges[j]->weight;
        // cout<<communicationCost<<" ";
        if (task->out_edges[j]->head->bottom_level == -1) {
            // cout<<"-1"<<endl;
            task->out_edges[j]->head->bottom_level = calculateSimpleBottomUpRank(task->out_edges[j]->head);
            // cout<<"then "<<task->out_edges[j]->head->bottom_level<<endl;
        }
        const double successorCost = task->out_edges[j]->head->bottom_level; // calculateSimpleBottomUpRank(task->out_edges[j]->head);
        double cost = communicationCost + successorCost;
        maxCost = std::max(maxCost, cost);
    }
    // cout<<endl;
    const double retur = (task->time + maxCost);
    task->bottom_level = retur;
    // cout<<"result "<<retur<<endl;
    return retur;
}

double calculateBLCBottomUpRank(const vertex_t* task)
{

    double maxCost = 0.0;
    for (int j = 0; j < task->out_degree; j++) {
        const double communicationCost = task->out_edges.at(j)->weight;
        const double successorCost = calculateBLCBottomUpRank(task->out_edges.at(j)->head);
        double cost = communicationCost + successorCost;
        maxCost = std::max(maxCost, cost);
    }
    const double simpleBl = task->time + maxCost;

    double maxInputCost = 0.0;
    for (int j = 0; j < task->in_degree; j++) {
        double communicationCost = task->in_edges.at(j)->weight;
        maxInputCost = std::max(maxInputCost, communicationCost);
    }
    const double retur = simpleBl + maxInputCost;
    return retur;
}

std::vector<std::pair<vertex_t*, double>> calculateMMBottomUpRank(graph_t* graphWMems)
{
    std::map<int, int> noMemToWithMem;
    graph_t* graph = convertToNonMemRepresentation(graphWMems, noMemToWithMem);
    // print_graph_to_cout(graph);

    SP_tree_t* sp_tree = nullptr;
    graph_t* sp_graph = nullptr;

    enforce_single_source_and_target(graph);
    sp_tree = build_SP_decomposition_tree(graph);
    if (sp_tree) {
        sp_graph = graph;
    } else {
        sp_graph = graph_sp_ization(graph);
        sp_tree = build_SP_decomposition_tree(sp_graph);
    }

    std::vector<std::pair<vertex_t*, int>> scheduleOnOriginal;

    if (sp_tree) {
        vertex_t** schedule = compute_optimal_SP_traversal(sp_graph, sp_tree);

        for (int i = 0; i < sp_graph->number_of_vertices; i++) {
            const vertex_t* vInSp = schedule[i];
            // cout<<vInSp->name<<endl;
            const std::map<int, int>::iterator& it = noMemToWithMem.find(vInSp->id);
            if (it != noMemToWithMem.end()) {
                vertex_t* vertexWithMem = graphWMems->vertices_by_id[(*it).second];
                if (std::find_if(scheduleOnOriginal.begin(), scheduleOnOriginal.end(),
                        [vertexWithMem](const std::pair<vertex_t*, int>& p) {
                            return p.first->name == vertexWithMem->name;
                        })
                    == scheduleOnOriginal.end()) {
                    scheduleOnOriginal.emplace_back(vertexWithMem,
                        sp_graph->number_of_vertices - i); // TODO: #vertices - i?
                }
            }
        }

    } else {
        throw std::runtime_error("No tree decomposition");
    }
    delete sp_tree;
    delete sp_graph;
    // delete graph;

    std::vector<std::pair<vertex_t*, double>> double_vector;

    // Convert each pair from (vertex_t*, int) to (vertex_t*, double)
    for (const auto& pair : scheduleOnOriginal) {
        double_vector.emplace_back(pair.first, static_cast<double>(pair.second));
    }

    return double_vector;
}

std::vector<std::pair<vertex_t*, double>> buildRanksWalkOver(graph_t* graph)
{
    std::vector<std::pair<vertex_t*, double>> ranks;
    enforce_single_source_and_target(graph);
    int rank = 0;
    vertex_t* vertex = graph->first_vertex;
    while (vertex != nullptr) {
        if (vertex->in_degree == 0) {
            ranks.emplace_back(vertex, rank);
        }
    }
    return ranks;
}

std::vector<std::pair<vertex_t*, double>> calculateBottomLevels(graph_t* graph, const int algoNum)
{
    std::vector<std::pair<vertex_t*, double>> ranks;
    switch (algoNum) {
    case fonda_scheduler::ALGORITHMS::HEFT:
    case fonda_scheduler::ALGORITHMS::HEFT_BL: {
        vertex_t* vertex = graph->first_vertex;
        while (vertex != nullptr) {
            double rank = calculateSimpleBottomUpRank(vertex);
            ranks.emplace_back(vertex, rank);
            vertex = vertex->next;
        }
        break;
    }
    case fonda_scheduler::ALGORITHMS::HEFT_BLC: {
        vertex_t* vertex = graph->first_vertex;
        while (vertex != nullptr) {
            double rank = calculateBLCBottomUpRank(vertex);
            ranks.emplace_back(vertex, rank);
            vertex = vertex->next;
        }
        break;
    }
    case fonda_scheduler::ALGORITHMS::HEFT_MM:
        ranks = calculateMMBottomUpRank(graph);
        break;
    default:
        throw std::runtime_error("unknown algorithm");
    }
    return ranks;
}

[[maybe_unused]] inline void checkIfPendingMemoryCorrect(const std::shared_ptr<Processor>& p)
{
    double sumOut = 0;
    for (const auto pendingMemorie : p->getPendingMemories()) {
        sumOut += pendingMemorie->weight;
    }
    const double busy = p->getAvailableMemory() + sumOut;
    if (abs(p->getMemorySize() - busy) > 0.1) {
        // cout << "check " << p->getMemorySize() << " vs " << busy << endl;
        p->setAvailableMemory(p->getAvailableMemory() + abs(p->getMemorySize() - busy));
    }

    assert(abs(p->getMemorySize() - busy) < 1);
    assert(p->getReadyTimeCompute() < std::numeric_limits<double>::max());
}

[[maybe_unused]] inline bool hasDuplicates(const std::vector<std::shared_ptr<Processor>>& vec)
{
    /*std::unordered_set<int> seenIds;
    for (const auto& obj : vec) {
        if (!seenIds.insert(obj->id).second) {
            // Insert returns {iterator, false} if the value already exists
            return true;
        }
    }
    return false; */
    return false;
}

std::shared_ptr<Processor> findProcessorThatHoldsEdge(edge_t* incomingEdge, Cluster* clusterToLookIn)
{
    for (auto& [proc_id, processor] : clusterToLookIn->getProcessors()) {
        auto iterator = std::find_if(processor->getPendingMemories().begin(), processor->getPendingMemories().end(),
            [incomingEdge](const edge_t* edge) {
                return incomingEdge == edge;
            });
        if (iterator != processor->getPendingMemories().end()) {
            return processor;
        }
    }
    return nullptr;
}
