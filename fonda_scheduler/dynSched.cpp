#include "fonda_scheduler/dynSched.hpp"

#include <iterator>


void evictAccordingToBestDecision(int &numberWithEvictedCases, SchedulingResult &bestSchedulingResult);

double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(const vertex_t *v, const shared_ptr<Processor> &pj) {
    assert(pj->getAvailableMemory() >= 0);
    double Res = pj->getAvailableMemory() - peakMemoryRequirementOfVertex(v);
    for (int i = 0; i < v->in_degree; i++) {
        auto inEdge = v->in_edges[i];
        if (pj->getPendingMemories().find(inEdge) != pj->getPendingMemories().end()) {
            //incoming edge occupied memory
            Res += inEdge->weight;
        }
    }
    return Res;
}

double new_heuristic(graph_t *graph, Cluster *cluster, int algoNum, bool isHeft) {
    algoNum = isHeft ? 1 : algoNum;
    vector<pair<vertex_t *, double>> ranks = calculateBottomLevels(graph, algoNum);
    removeSourceAndTarget(graph, ranks);
    sort(ranks.begin(), ranks.end(),
         [](pair<vertex_t *, double> a, pair<vertex_t *, double> b) {
             return a.second > b.second;

         });
    double makespan = 0;
    int numberWithEvictedCases = 0;
    for (auto &pair: ranks) {
        auto vertex = pair.first;
         cout<<"processing "<< vertex->name<<endl;
        SchedulingResult bestSchedulingResult = SchedulingResult(nullptr);
        bestTentativeAssignment(cluster, isHeft, vertex, bestSchedulingResult);


        if (bestSchedulingResult.modifiedProcs.empty()) {
            cout << "Invalid assignment of " << vertex->name;
            return -1;
        } else {
            cout << " for " << vertex->name << " best " << bestSchedulingResult.startTime << " " << bestSchedulingResult.finishTime << " on proc "
                 << bestSchedulingResult.processorOfAssignment->id << endl;//<<" with av mem "<<bestProcessorToAssign->availableMemory<<endl;
        }


        evictAccordingToBestDecision(numberWithEvictedCases, bestSchedulingResult);
        // checkIfPendingMemoryCorrect(bestProcessorToAssign);

        if(vertex->name=="bismark_align_00001479" || vertex->name=="trim_galore_00001440"|| vertex->name=="bismark_align_00001947"){
            cout<<endl;
        }

        for (auto &modifiedProc: bestSchedulingResult.modifiedProcs) {
            // cout<<" from "<<modifiedProc->id<<endl;
            auto procInClusterWithId = cluster->getProcessorById(modifiedProc->id);

            //if ((procInClusterWithId->id != bestSchedulingResult.processorOfAssignment->id || isHeft) &&
           //     procInClusterWithId->getPendingMemories().size() != modifiedProc->getPendingMemories().size()) {
           procInClusterWithId->updateFrom(*modifiedProc);
            //}
        }
        //  cout<<"delocating from other procs according to tentative "<<endl;
        // bestProcessorToAssign->assignment+="started at "+ to_string(bestStartTime)+"; ";
      /*  for (auto &modifiedProc: bestSchedulingResult.modifiedProcs) {
            // cout<<" from "<<modifiedProc->id<<endl;
            auto procInClusterWithId = cluster->getProcessorById(modifiedProc->id);

            if ((procInClusterWithId->id != bestSchedulingResult.processorOfAssignment->id || isHeft) &&
                procInClusterWithId->getPendingMemories().size() != modifiedProc->getPendingMemories().size()) {

                for (auto pendingInCluster = procInClusterWithId->getPendingMemories().begin();
                     pendingInCluster != procInClusterWithId->getPendingMemories().end();) {
                    //  print_edge(*pendingInCluster);
                    auto i = std::find_if(modifiedProc->getPendingMemories().begin(),
                                          modifiedProc->getPendingMemories().end(),
                                          [&pendingInCluster](auto pendInModif) {
                                              return pendInModif == *pendingInCluster;
                                          });
                    if (i == modifiedProc->getPendingMemories().end()) {
                        delocateFromThisProcessorToDisk(*pendingInCluster, modifiedProc->id);
                        //if((*pendingInCluster)->head->name== vertex->name || !(isHeft && procInClusterWithId->id== bestProcessorToAssign->id ) ) {
                        //    cout<<"increased available memory from "<< modifiedProc->availableMemory;
                        //    modifiedProc->availableMemory += (*pendingInCluster)->weight;
                        //    cout<<" to "<<modifiedProc->availableMemory<<endl;
                        //}
                    }
                    pendingInCluster++;
                }

            }
            assert(modifiedProc->getReadyTimeCompute() < std::numeric_limits<double>::max());
            //  cout<<"erplacing "<<modifiedProc->id<<endl;
            checkIfPendingMemoryCorrect(modifiedProc);
            //   modifiedProc->assignment+= " ready time "+ to_string(modifiedProc->readyTimeCompute)+" write "+
            //          to_string(modifiedProc->readyTimeWrite) + " read "+ to_string(modifiedProc->readyTimeRead)+ "; ";
            cluster->replaceProcessor(modifiedProc);


        } */
        assert(bestSchedulingResult.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
        vertex->assignedProcessorId = bestSchedulingResult.processorOfAssignment->id;

        //checkIfPendingMemoryCorrect(bestProcessorToAssign);
        for (int j = 0; j < vertex->in_degree; j++) {
            edge *ine = vertex->in_edges[j];
           // int processorId = whatProcessorIsLocatedOn(ine);
            //if(processorId!=-1){
             //  cluster->getProcessorById( processorId)->delocateToDisk(ine);
           // }

            assert(whatProcessorIsLocatedOn(ine) ==-1 ||whatProcessorIsLocatedOn(ine) == bestSchedulingResult.processorOfAssignment->id ||
                    cluster->getProcessorById( whatProcessorIsLocatedOn(ine))->getPendingMemories().find(ine)
            == cluster->getProcessorById( whatProcessorIsLocatedOn(ine))->getPendingMemories().end());

            if(whatProcessorIsLocatedOn(ine) == bestSchedulingResult.processorOfAssignment->id){
                bestSchedulingResult.processorOfAssignment->delocateToDisk(ine);
            }
            ine->locations.clear();
        }

        //  cout<<"kicking in edges"<<endl;
       /* for (int j = 0; j < vertex->in_degree; j++) {
            edge *ine = vertex->in_edges[j];
            //    cout<<"ine ";print_edge(ine);
            for (auto location: ine->locations) {
                if (location.locationType ==
                    LocationType::OnProcessor) { //&& location.processorId.value()!=bestProcessorToAssign->id
                    assert(location.processorId.has_value());
                    shared_ptr<Processor> otherProc = cluster->getProcessorById(location.processorId.value());
                    auto it = std::find_if(otherProc->getPendingMemories().begin(),
                                           otherProc->getPendingMemories().end(),
                                           [&ine](edge_t *e) {
                                               return e->head->id == ine->head->id && e->tail->id == ine->tail->id;
                                           });
                    if (it != otherProc->getPendingMemories().end()) {
                        //   cout<<"found and kicked from "<<otherProc->id<<endl;
                        otherProc->delocateToDisk(ine);
                    } else {
                        cout << "NOT FOUND ";
                        print_edge(ine);
                        //cout<<"IN ";
                        // for ( auto edge: otherProc->pendingMemories){
                        //     print_edge(edge);
                        //}
                        //cout<<endl;
                        cout << "ON PROC " << otherProc->id << endl;
                        throw runtime_error("not found in mems");
                    }
                }
            }
            ine->locations.clear();//.emplace_back(LocationType::OnProcessor, bestProcessorToAssign->id);
        } */
        checkIfPendingMemoryCorrect(bestSchedulingResult.processorOfAssignment);

        //cout<<"emplacing out edges , starting with "<<bestProcessorToAssign->availableMemory<<endl;
        for (int i = 0; i < vertex->out_degree; i++) {
            auto v1 = vertex->out_edges[i];
            // print_edge(v1);
            //  cout<<"adding "<<v1->weight<<endl;
            bestSchedulingResult.processorOfAssignment->loadFromNowhere(v1);
            checkIfPendingMemoryCorrect(bestSchedulingResult.processorOfAssignment);

        }
        cluster->getProcessorById(bestSchedulingResult.processorOfAssignment->id)->updateFrom(*bestSchedulingResult.processorOfAssignment);
        for (const auto &item: cluster->getProcessors()) {
            checkIfPendingMemoryCorrect(item.second);
        }


        vertex->makespan = bestSchedulingResult.finishTime;
        assert(bestSchedulingResult.startTime < bestSchedulingResult.finishTime);
        //cluster->printProcessors();
        if (makespan < bestSchedulingResult.finishTime)
            makespan = bestSchedulingResult.finishTime;
    }
    cout << " #eviction " << numberWithEvictedCases << " ";
    return makespan;
}

void evictAccordingToBestDecision(int &numberWithEvictedCases, SchedulingResult &bestSchedulingResult) {
    switch (bestSchedulingResult.resultingVar) {
        case 1:
            break;
        case 2:
            //cout<<"best with 1 kick"<<endl;
            assert(bestSchedulingResult.edgeToKick != nullptr);
            bestSchedulingResult.processorOfAssignment->delocateToDisk(bestSchedulingResult.edgeToKick);
            numberWithEvictedCases++;
            checkIfPendingMemoryCorrect(bestSchedulingResult.processorOfAssignment);
            break;
        case 3:
            //cout<<"best with all kick"<<endl;
            for (auto it = bestSchedulingResult.processorOfAssignment->getPendingMemories().begin();
                 it != bestSchedulingResult.processorOfAssignment->getPendingMemories().end();) {
                it = bestSchedulingResult.processorOfAssignment->delocateToDisk(*it);
                //cout<<"end deloc"<<endl;
            }
            assert(bestSchedulingResult.processorOfAssignment->getPendingMemories().empty());
            numberWithEvictedCases++;
            checkIfPendingMemoryCorrect(bestSchedulingResult.processorOfAssignment);
            break;
        default:
            throw runtime_error("");
    }    
}

void bestTentativeAssignment(Cluster *cluster, bool isHeft, vertex_t *vertex, SchedulingResult &result) {
    result.finishTime = numeric_limits<double>::max();
    result.startTime = 0;

    double bestActualStartTime, bestActualFinishTime;
    for (auto &[id, processor]: cluster->getProcessors()) {

        double actualStartTime = 0, actualFinishTime = 0;
        double ftBefore = processor->getReadyTimeCompute();


        SchedulingResult tentativeResult = SchedulingResult(processor);

        checkIfPendingMemoryCorrect(processor);
        if (isHeft) {
            tentativeAssignmentHEFT(vertex, cluster, tentativeResult,
                                    actualFinishTime,
                                    actualStartTime);
        } else {
            tentativeAssignment(vertex, cluster, tentativeResult);
        }

        if (!isHeft)
            checkIfPendingMemoryCorrect(tentativeResult.processorOfAssignment);
        // cout<<"start "<<startTime<<" end "<<finTime<<endl;
        if (tentativeResult.startTime != actualStartTime) {
            //  cout<<"not actual "<<finTime<<" vs "<<actualFinishTime<<" on "<<processor->id<<endl;
        }
        if (result.finishTime > tentativeResult.finishTime
            || (result.finishTime == tentativeResult.finishTime &&
                result.processorOfAssignment && tentativeResult.processorOfAssignment->getMemorySize() >
                                                result.processorOfAssignment->getMemorySize())) {
            //    cout<<"best acutalize "<<endl;
            assert(!tentativeResult.modifiedProcs.empty());
            result = tentativeResult;
            if (isHeft) {
                bestActualFinishTime = actualFinishTime;
                bestActualStartTime = actualStartTime;
            }
        }
    }

    if (isHeft) {

        // cout<<"best times: "<<bestStartTime<<" "<<bestFinishTime<<" "<<bestActualStartTime<<" "<<bestActualFinishTime<<(bestStartTime!=bestActualStartTime?" neq ":" eq ") <<" on "<<bestProcessorToAssign->id<<endl;
        assert(result.startTime <= bestActualStartTime);
        if (result.startTime != bestActualStartTime) {
            //numberWithEvictedCases++;
            //throw runtime_error("numberWithEvictedCases++;");
            cout<<"increase numWithEvictged in HEFT"<<endl;
        }
        result.finishTime = bestActualFinishTime;
        result.startTime = bestActualStartTime;
        result.resultingVar = 1;
    }
}

void
tentativeAssignment(vertex_t *v, Cluster *cluster, SchedulingResult &result) {

    //cout<<"tent on proc "<<ourModifiedProc->id<< " ";
    //  assert(ourModifiedProc->readyTimeCompute<  std::numeric_limits<double>::max());
    result.resultingVar = 1;

    double sumOut = getSumOut(v);
    if (result.processorOfAssignment->getMemorySize() < sumOut) {
        //  cout<<"too large outs absolutely"<<endl;
        result.finishTime = std::numeric_limits<double>::max();
        return;
    }
    realSurplusOfOutgoingEdges(v, result.processorOfAssignment, sumOut);

    vector<std::shared_ptr<Processor> > modifiedProcs;
    modifiedProcs.emplace_back(result.processorOfAssignment);
    processIncomingEdges(v, result.processorOfAssignment, modifiedProcs, result.startTime, cluster);


    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    result.startTime = result.processorOfAssignment->getReadyTimeCompute() > result.startTime
                       ? result.processorOfAssignment->getReadyTimeCompute() : result.startTime;

    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    checkIfPendingMemoryCorrect(result.processorOfAssignment);

    double Res = howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(v, result.processorOfAssignment);
    result.peakMem = (Res < 0) ? 1 : (result.processorOfAssignment->getMemorySize() - Res) /
                                     result.processorOfAssignment->getMemorySize();

    if (Res < 0) {
        //try finish times with and without memory overflow
        double amountToOffload = -Res;
        double shortestFT = std::numeric_limits<double>::max();

        double timeToFinishNoEvicted = result.startTime + v->time / result.processorOfAssignment->getProcessorSpeed() +
                                       amountToOffload / result.processorOfAssignment->memoryOffloadingPenalty;
        assert(timeToFinishNoEvicted > result.startTime);
        if (sumOut > result.processorOfAssignment->getAvailableMemory()) {
            //cout<<"cant"<<endl;
            timeToFinishNoEvicted = std::numeric_limits<double>::max();
        }


        double timeToFinishBiggestEvicted = std::numeric_limits<double>::max(),
                timeToFinishAllEvicted = std::numeric_limits<double>::max();
        double timeToWriteAllPending = 0;

        double startTimeFor1Evicted, startTimeForAllEvicted;
        startTimeFor1Evicted = startTimeForAllEvicted =
                result.processorOfAssignment->getReadyTimeWrite() > result.startTime ?
                result.processorOfAssignment->getReadyTimeWrite() : result.startTime;
        if (!result.processorOfAssignment->getPendingMemories().empty()) {
            assert((*result.processorOfAssignment->getPendingMemories().begin())->weight >=
                   (*result.processorOfAssignment->getPendingMemories().rbegin())->weight);
            auto biggestFileWeight = (*result.processorOfAssignment->getPendingMemories().begin())->weight;
            double amountToOffloadWithoutBiggestFile = (amountToOffload - biggestFileWeight) > 0 ? (amountToOffload -
                                                                                                    biggestFileWeight)
                                                                                                 : 0;
            double finishTimeToWrite = result.processorOfAssignment->getReadyTimeWrite() +
                                       biggestFileWeight / result.processorOfAssignment->writeSpeedDisk;
            startTimeFor1Evicted = max(result.startTime, finishTimeToWrite);
            timeToFinishBiggestEvicted =
                    startTimeFor1Evicted
                    + v->time / result.processorOfAssignment->getProcessorSpeed() +
                    amountToOffloadWithoutBiggestFile / result.processorOfAssignment->memoryOffloadingPenalty;
            assert(timeToFinishBiggestEvicted > startTimeFor1Evicted);

            double availableMemWithoutBiggest = result.processorOfAssignment->getAvailableMemory() + biggestFileWeight;
            if (sumOut > availableMemWithoutBiggest)
                timeToFinishBiggestEvicted = std::numeric_limits<double>::max();


            double sumWeightsOfAllPending = 0;
            for (const auto &item: result.processorOfAssignment->getPendingMemories()) {
                timeToWriteAllPending += item->weight / result.processorOfAssignment->writeSpeedDisk;
                sumWeightsOfAllPending += item->weight;
            }

            double amountToOffloadWithoutAllFiles = (amountToOffload - sumWeightsOfAllPending > 0) ?
                                                    amountToOffload - sumWeightsOfAllPending : 0;

            assert(amountToOffloadWithoutAllFiles >= 0);
            finishTimeToWrite = result.processorOfAssignment->getReadyTimeWrite() +
                                timeToWriteAllPending;
            startTimeForAllEvicted = max(startTimeForAllEvicted, finishTimeToWrite);
            timeToFinishAllEvicted =
                    startTimeForAllEvicted + v->time / result.processorOfAssignment->getProcessorSpeed() +
                    amountToOffloadWithoutAllFiles / result.processorOfAssignment->memoryOffloadingPenalty;
            assert(timeToFinishAllEvicted > startTimeForAllEvicted);

        }

        double minTTF = min(timeToFinishNoEvicted, min(timeToFinishBiggestEvicted, timeToFinishAllEvicted));
        if (minTTF == std::numeric_limits<double>::max()) {
            cout << "minTTF inf" << endl;
            result.finishTime = std::numeric_limits<double>::max();
            return;
        }
        assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
        result.processorOfAssignment->setReadyTimeCompute(minTTF);
        result.finishTime = result.processorOfAssignment->getReadyTimeCompute();
        assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());

        if (timeToFinishBiggestEvicted == minTTF) {
            result.edgeToKick = (*result.processorOfAssignment->getPendingMemories().begin());
            //cout<<"best tentative with biggest Evicted "; print_edge(toKick);
            result.resultingVar = 2;
            result.processorOfAssignment->setReadyTimeWrite(result.processorOfAssignment->getReadyTimeWrite() +
                                                            (*result.processorOfAssignment->getPendingMemories().begin())->weight /
                                                            result.processorOfAssignment->writeSpeedDisk);
            // ourModifiedProc->pendingMemories.erase()
            //penMemsAsVector.erase(penMemsAsVector.begin());
            assert(result.startTime <= startTimeFor1Evicted);
            result.startTime = startTimeFor1Evicted;
            assert(result.edgeToKick != nullptr);
            assert(!result.edgeToKick->locations.empty());
            assert(isLocatedOnThisProcessor(result.edgeToKick, result.processorOfAssignment->id));
        }
        if (timeToFinishAllEvicted == minTTF) {
            result.resultingVar = 3;
            // cout<<"best tentative with all Evicted ";
            result.processorOfAssignment->setReadyTimeWrite(
                    result.processorOfAssignment->getReadyTimeWrite() + timeToWriteAllPending);
            assert(result.startTime <= startTimeForAllEvicted);
            result.startTime = startTimeForAllEvicted;
            //penMemsAsVector.resize(0);

        }


    } else {
        //startTime =  ourModifiedProc->readyTimeCompute;
        // printInlineDebug("should be successful");
        result.processorOfAssignment->setReadyTimeCompute(
                result.startTime + v->time / result.processorOfAssignment->getProcessorSpeed());
        result.finishTime = result.processorOfAssignment->getReadyTimeCompute();

    }
    // cout<<endl;
    assert(result.finishTime > result.startTime);
    result.modifiedProcs = modifiedProcs;
}


void
tentativeAssignmentHEFT(vertex_t *v, Cluster *cluster, SchedulingResult &result, double &actualFinishTime,
                        double &actualStartTime) {

    //cout<<"tent on proc "<<ourModifiedProc->id<< " ";
    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());


    double sumOut = getSumOut(v);
    bool kicked = false;

    if (result.processorOfAssignment->getMemorySize() < sumOut) {
        //  cout<<"too large outs absolutely"<<endl;
        result.finishTime = actualFinishTime = std::numeric_limits<double>::max();
        return;
    }

    // cout<<"sumOut includes ";
    realSurplusOfOutgoingEdges(v, result.processorOfAssignment, sumOut);


    vector<std::shared_ptr<Processor> > modifiedProcs;
    modifiedProcs.emplace_back(result.processorOfAssignment);
    processIncomingEdges(v, result.processorOfAssignment, modifiedProcs, result.startTime, cluster);


    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    actualStartTime = result.startTime = result.processorOfAssignment->getReadyTimeCompute() > result.startTime ?
                                         result.processorOfAssignment->getReadyTimeCompute() : result.startTime;

    assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
    double initAvM = result.processorOfAssignment->getAvailableMemory();

    if (result.processorOfAssignment->getAvailableMemory() < sumOut) {
        kicked = true;
        // cout<<"sum out is "<<sumOut <<", kicking unexpectedly "<<endl;
        double stillNeedsToBeEvictedToRun = sumOut - result.processorOfAssignment->getAvailableMemory();
        double writeTime = actualStartTime > result.processorOfAssignment->getReadyTimeCompute() ? actualStartTime
                                                                                                 : result.processorOfAssignment->getReadyTimeCompute();

        for (auto it = result.processorOfAssignment->getPendingMemories().begin();
             it != result.processorOfAssignment->getPendingMemories().end() && stillNeedsToBeEvictedToRun > 0;) {
            //  print_edge(*it);
            if ((*it)->head->name != v->name) {
                stillNeedsToBeEvictedToRun -= (*it)->weight;
                writeTime += (*it)->weight / result.processorOfAssignment->writeSpeedDisk;
                it = result.processorOfAssignment->removePendingMemory(*it);
            } else { ++it; }

        }
        if (stillNeedsToBeEvictedToRun > 0) {

        }
        assert(stillNeedsToBeEvictedToRun <= 0);
        assert(result.processorOfAssignment->getAvailableMemory() >= sumOut);
        actualStartTime = writeTime;
        result.processorOfAssignment->setReadyTimeWrite(writeTime);
        result.processorOfAssignment->setReadyTimeCompute(writeTime);
        assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
        // cout<<"ednded up with "<<ourModifiedProc->availableMemory<<endl;
        //checkIfPendingMemoryCorrect(ourModifiedProc);


        //   cout<<"assuming that remain pending: "<<ourModifiedProc->pendingMemories.size()<<" pieces, with avail memory "<<ourModifiedProc->availableMemory<< " ";
        initAvM = result.processorOfAssignment->getAvailableMemory();
        // for (auto it = ourModifiedProc->pendingMemories.begin();
        //      it != ourModifiedProc->pendingMemories.end();){
        //      print_edge(*it);
        //     it++;
        //  }

        //  cout<<endl;

    }

    double Res = howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(v, result.processorOfAssignment);
    result.peakMem = (Res < 0) ? 1 : (result.processorOfAssignment->getMemorySize() - Res) /
                                     result.processorOfAssignment->getMemorySize();

    if (Res < 0) {
        //try finish times with and without memory overflow
        double amountToOffload = -Res;

        result.finishTime = result.startTime + v->time / result.processorOfAssignment->getProcessorSpeed() +
                            amountToOffload / result.processorOfAssignment->memoryOffloadingPenalty;
        actualFinishTime = actualStartTime + v->time / result.processorOfAssignment->getProcessorSpeed() +
                           amountToOffload / result.processorOfAssignment->memoryOffloadingPenalty;
        assert(result.finishTime > result.startTime);


        if (result.finishTime == std::numeric_limits<double>::max()) {
            cout << "perceivedFinishTime inf" << endl;
            actualFinishTime = std::numeric_limits<double>::max();
            return;
        }
        assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
        result.processorOfAssignment->setReadyTimeCompute(actualFinishTime);
        assert(result.processorOfAssignment->getReadyTimeCompute() < std::numeric_limits<double>::max());
        assert(result.startTime <= actualStartTime);
        assert(result.finishTime <= actualFinishTime);
        if (kicked) {
            assert(result.startTime < actualStartTime);
            assert(result.finishTime < actualFinishTime);
        }

        result.modifiedProcs = modifiedProcs;

    } else {
        //startTime =  ourModifiedProc->readyTimeCompute;
        // printInlineDebug("should be successful");
        result.processorOfAssignment->setReadyTimeCompute(
                actualStartTime + v->time / result.processorOfAssignment->getProcessorSpeed());
        actualFinishTime = result.processorOfAssignment->getReadyTimeCompute();
        result.finishTime = actualFinishTime;
    }
    //    cout<<endl;
    assert(actualFinishTime > actualStartTime);
    result.modifiedProcs = modifiedProcs;
}

void realSurplusOfOutgoingEdges(const vertex_t *v, shared_ptr<Processor> &ourModifiedProc, double &sumOut) {
    for (int i = 0; i < v->in_degree; i++) {
        auto inEdge = v->in_edges[i];
        if (isLocatedOnThisProcessor(inEdge, ourModifiedProc->id)) {
            //     cout<<"in is located here "; print_edge(v->in_edges[i]);
            auto pendingOfProc = ourModifiedProc->getPendingMemories();
            //assert(pendingOfProc.find(inEdge) != pendingOfProc.end());
            if (pendingOfProc.find(inEdge) != pendingOfProc.end()) {
                sumOut -= inEdge->weight;
            } else {
                // cout<<"edge "<<buildEdgeName(inEdge)<<" not anymore found in pending mems of processor "<<ourModifiedProc->id<<endl;
            }

        }

    }
    //  cout << "REQUIRES AT THE END: " << sumOut << endl;
}


void
processIncomingEdges(const vertex_t *v, shared_ptr<Processor> &ourModifiedProc,
                     vector<std::shared_ptr<Processor>> &modifiedProcs,
                     double &earliestStartingTimeToComputeVertex, Cluster *cluster) {
    earliestStartingTimeToComputeVertex = ourModifiedProc->getReadyTimeCompute();
    for (int j = 0; j < v->in_degree; j++) {
        edge *incomingEdge = v->in_edges[j];
        vertex_t *predecessor = incomingEdge->tail;

        if (predecessor->assignedProcessorId == ourModifiedProc->id) {
            if (!isLocatedOnThisProcessor(incomingEdge, ourModifiedProc->id)) {
                assert(isLocatedOnDisk(incomingEdge));
                ourModifiedProc->setReadyTimeRead(
                        ourModifiedProc->getReadyTimeRead() + incomingEdge->weight / ourModifiedProc->readSpeedDisk);
                earliestStartingTimeToComputeVertex =
                        ourModifiedProc->getReadyTimeRead() > earliestStartingTimeToComputeVertex ?
                        ourModifiedProc->getReadyTimeRead() : earliestStartingTimeToComputeVertex;
            }

        } else {
            if (isLocatedOnDisk(incomingEdge)) {
                //we need to schedule read
                ourModifiedProc->setReadyTimeRead(
                        ourModifiedProc->getReadyTimeRead() + incomingEdge->weight / ourModifiedProc->readSpeedDisk);
                earliestStartingTimeToComputeVertex =
                        ourModifiedProc->getReadyTimeRead() > earliestStartingTimeToComputeVertex ?
                        ourModifiedProc->getReadyTimeRead() : earliestStartingTimeToComputeVertex;
                //TODO evict??
            } else {
                auto predecessorsProcessorsId = predecessor->assignedProcessorId;
                assert(isLocatedOnThisProcessor(incomingEdge, predecessorsProcessorsId));
                shared_ptr<Processor> addedProc;
                auto it = //modifiedProcs.size()==1?
                        //  modifiedProcs.begin():
                        std::find_if(modifiedProcs.begin(), modifiedProcs.end(),
                                     [predecessorsProcessorsId](const shared_ptr<Processor> &p) {
                                         return p->id == predecessorsProcessorsId;
                                     });

                if (it == modifiedProcs.end()) {
                    addedProc = make_shared<Processor>(*cluster->getProcessorById(predecessorsProcessorsId));
                    // cout<<"adding modified proc "<<addedProc->id<<endl;
                    modifiedProcs.emplace_back(addedProc);
                    checkIfPendingMemoryCorrect(addedProc);
                } else {
                    addedProc = *it;
                }

                assert(!hasDuplicates(modifiedProcs));

                double timeToStartWriting = max(predecessor->makespan, addedProc->getReadyTimeWrite());
                addedProc->setReadyTimeWrite(timeToStartWriting + incomingEdge->weight / addedProc->writeSpeedDisk);
                double startTimeOfRead = max(addedProc->getReadyTimeWrite(), ourModifiedProc->getReadyTimeRead());
                double endTimeOfRead = startTimeOfRead + incomingEdge->weight / ourModifiedProc->readSpeedDisk;
                ourModifiedProc->setReadyTimeRead(endTimeOfRead);

                earliestStartingTimeToComputeVertex = max(earliestStartingTimeToComputeVertex, endTimeOfRead);
                //int addpl  = addedProc->pendingMemories.size();
                addedProc->removePendingMemory(incomingEdge);
                // assert(addpl> addedProc->pendingMemories.size());
                checkIfPendingMemoryCorrect(addedProc);
            }
        }
    }
}


graph_t *convertToNonMemRepresentation(graph_t *withMemories, map<int, int> &noMemToWithMem) {
    enforce_single_source_and_target(withMemories);
    graph_t *noNodeMemories = new_graph();

    for (vertex_t *vertex = withMemories->source; vertex; vertex = next_vertex_in_sorted_topological_order(withMemories,
                                                                                                           vertex,
                                                                                                           &sort_by_increasing_top_level)) {
        vertex_t *invtx = new_vertex(noNodeMemories, vertex->name + "-in", vertex->time, nullptr);
        noMemToWithMem.insert({invtx->id, vertex->id});
        if (!noNodeMemories->source) {
            noNodeMemories->source = invtx;
        }
        vertex_t *outvtx = new_vertex(noNodeMemories, vertex->name + "-out", 0.0, nullptr);
        noMemToWithMem.insert({outvtx->id, vertex->id});
        edge_t *e = new_edge(noNodeMemories, invtx, outvtx, vertex->memoryRequirement, nullptr);
        noNodeMemories->target = outvtx;

        for (int i = 0; i < vertex->in_degree; i++) {
            edge *inEdgeOriginal = vertex->in_edges[i];
            string expectedName = inEdgeOriginal->tail->name + "-out";
            vertex_t *outVtxOfCopiedInVtxOfEdge = findVertexByName(noNodeMemories, expectedName);

            if (outVtxOfCopiedInVtxOfEdge == nullptr) {
                print_graph_to_cout(noNodeMemories);
                outVtxOfCopiedInVtxOfEdge = findVertexByName(noNodeMemories, expectedName);
                cout << "expected: " << expectedName << endl;
                throw std::invalid_argument(" no vertex found for expected name.");

            }
            edge_t *e_new = new_edge(noNodeMemories, outVtxOfCopiedInVtxOfEdge, invtx, inEdgeOriginal->weight, nullptr);
        }
    }

    return noNodeMemories;
}

double calculateSimpleBottomUpRank(vertex_t *task) {
//    cout<<"rank for "<<task->name<<" ";



    double maxCost = 0.0;
    for (int j = 0; j < task->out_degree; j++) {
        double communicationCost = task->out_edges[j]->weight;
        // cout<<communicationCost<<" ";
        if (task->out_edges[j]->head->bottom_level == -1) {
            //cout<<"-1"<<endl;
            task->out_edges[j]->head->bottom_level = calculateSimpleBottomUpRank(task->out_edges[j]->head);
            // cout<<"then "<<task->out_edges[j]->head->bottom_level<<endl;
        }
        double successorCost = task->out_edges[j]->head->bottom_level; //calculateSimpleBottomUpRank(task->out_edges[j]->head);
        double cost = communicationCost + successorCost;
        maxCost = max(maxCost, cost);
    }
    //cout<<endl;
    double retur = (task->time + maxCost);
    task->bottom_level = retur;
    // cout<<"result "<<retur<<endl;
    return retur;
}

double calculateBLCBottomUpRank(vertex_t *task) {

    double maxCost = 0.0;
    for (int j = 0; j < task->out_degree; j++) {
        double communicationCost = task->out_edges[j]->weight;
        double successorCost = calculateBLCBottomUpRank(task->out_edges[j]->head);
        double cost = communicationCost + successorCost;
        maxCost = max(maxCost, cost);
    }
    double simpleBl = task->time + maxCost;

    double maxInputCost = 0.0;
    for (int j = 0; j < task->in_degree; j++) {
        double communicationCost = task->in_edges[j]->weight;
        maxInputCost = max(maxInputCost, communicationCost);
    }
    double retur = simpleBl + maxInputCost;
    return retur;
}

std::vector<std::pair<vertex_t *, double> > calculateMMBottomUpRank(graph_t *graphWMems) {

    map<int, int> noMemToWithMem;
    graph_t *graph = convertToNonMemRepresentation(graphWMems, noMemToWithMem);
    // print_graph_to_cout(graph);

    SP_tree_t *sp_tree = nullptr;
    graph_t *sp_graph = nullptr;

    enforce_single_source_and_target(graph);
    sp_tree = build_SP_decomposition_tree(graph);
    if (sp_tree) {
        sp_graph = graph;
    } else {
        sp_graph = graph_sp_ization(graph);
        sp_tree = build_SP_decomposition_tree(sp_graph);
    }


    std::vector<std::pair<vertex_t *, int> > scheduleOnOriginal;

    if (sp_tree) {
        vertex_t **schedule = compute_optimal_SP_traversal(sp_graph, sp_tree);

        for (int i = 0; i < sp_graph->number_of_vertices; i++) {
            vertex_t *vInSp = schedule[i];
            //cout<<vInSp->name<<endl;
            const map<int, int>::iterator &it = noMemToWithMem.find(vInSp->id);
            if (it != noMemToWithMem.end()) {
                vertex_t *vertexWithMem = graphWMems->vertices_by_id[(*it).second];
                if (std::find_if(scheduleOnOriginal.begin(), scheduleOnOriginal.end(),
                                 [vertexWithMem](std::pair<vertex_t *, int> p) {
                                     return p.first->name == vertexWithMem->name;
                                 }) == scheduleOnOriginal.end()) {
                    scheduleOnOriginal.emplace_back(vertexWithMem,
                                                    sp_graph->number_of_vertices - i);// TODO: #vertices - i?
                }
            }
        }

    } else {
        throw runtime_error("No tree decomposition");
    }
    delete sp_tree;
    delete sp_graph;
    //delete graph;


    std::vector<std::pair<vertex_t *, double>> double_vector;

    // Convert each pair from (vertex_t*, int) to (vertex_t*, double)
    for (const auto &pair: scheduleOnOriginal) {
        double_vector.emplace_back(pair.first, static_cast<double>(pair.second));
    }

    return double_vector;
}

vector<pair<vertex_t *, double>> buildRanksWalkOver(graph_t *graph) {
    vector<pair<vertex_t *, double> > ranks;
    enforce_single_source_and_target(graph);
    int rank = 0;
    vertex_t *vertex = graph->first_vertex;
    while (vertex != nullptr) {
        if (vertex->in_degree == 0) {
            ranks.emplace_back(vertex, rank);
        }
    }
    for (auto &item: ranks) {
        for (int i = 0; i < item.first->out_degree; i++) {
            //    if(find)
        }
    }

}

vector<pair<vertex_t *, double>> calculateBottomLevels(graph_t *graph, int bottomLevelVariant) {
    vector<pair<vertex_t *, double> > ranks;
    switch (bottomLevelVariant) {
        case 1: {
            vertex_t *vertex = graph->first_vertex;
            while (vertex != nullptr) {
                double rank = calculateSimpleBottomUpRank(vertex);
                //cout<<"rank for "<<vertex->name<<" is "<<rank<<endl;
                ranks.emplace_back(vertex, rank);
                vertex = vertex->next;
            }
            break;
        }
        case 2: {
            vertex_t *vertex = graph->first_vertex;
            while (vertex != nullptr) {
                double rank = calculateBLCBottomUpRank(vertex);
                ranks.emplace_back(vertex, rank);
                vertex = vertex->next;
            }
            break;
        }
        case 3:
            ranks = calculateMMBottomUpRank(graph);
            break;
        default:
            throw runtime_error("unknon algorithm");
    }
    return ranks;
}

[[maybe_unused]] inline void checkIfPendingMemoryCorrect(const shared_ptr<Processor> &p) {
    /*  double sumOut=0;
      for(auto pendingMemorie : p->pendingMemories){
          sumOut +=pendingMemorie->weight;
      }
      double busy = p->availableMemory + sumOut;
      if(abs(p->getMemorySize() -busy) >0.1)
          cout<<"check "<<p->getMemorySize()<<" vs "<< busy<<endl;
      assert(abs(p->getMemorySize() -busy) <0.1);
      assert(p->readyTimeCompute<  std::numeric_limits<double>::max()); */
}

[[maybe_unused]] inline bool hasDuplicates(const std::vector<shared_ptr<Processor>> &vec) {
    /*std::unordered_set<int> seenIds;
    for (const auto& obj : vec) {
        if (!seenIds.insert(obj->id).second) {
            // Insert returns {iterator, false} if the value already exists
            return true;
        }
    }
    return false; */ return false;
}