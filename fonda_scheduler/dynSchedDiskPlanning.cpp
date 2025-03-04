#include "fonda_scheduler/dynSchedDisk.hpp"
#include "fonda_scheduler/dynSched.hpp"

vector<shared_ptr<Event>> bestTentativeAssignment(vertex_t *vertex, vector<shared_ptr<Processor>> &bestModifiedProcs,
                                                  shared_ptr<Processor> &bestProcessorToAssign, double notEarlierThan) {
    //cout << "!!!START BEST tent assign for " << vertex->name << endl;
    double bestStartTime = numeric_limits<double>::max(), bestFinishTime = numeric_limits<double>::max(),
            bestReallyUsedMem;
    vector<shared_ptr<Event>> bestEvents;
    double resultingVar;

    for (auto &[id, processor]: cluster->getProcessors()) {
        double finTime = -1, startTime = -1, reallyUsedMem = 0;
        double ftBefore = processor->getReadyTimeCompute();
        int resultingEvictionVariant = -1;
        auto ourModifiedProc = make_shared<Processor>(*processor);
        vector<shared_ptr<Event>> newEvents = {};
        //checkIfPendingMemoryCorrect(ourModifiedProc);
        vector<shared_ptr<Processor>> modifiedProcs = tentativeAssignment(vertex, ourModifiedProc,
                                                                          finTime,
                                                                          startTime, resultingEvictionVariant,
                                                                          newEvents, reallyUsedMem, notEarlierThan);

        if (bestFinishTime > finTime) {
            //cout << "best acutalize to " << ourModifiedProc->id << " act used mem " << reallyUsedMem << endl;
            assert(!modifiedProcs.empty());
            bestModifiedProcs = modifiedProcs;
            bestFinishTime = finTime;
            bestStartTime = startTime;
            bestProcessorToAssign = ourModifiedProc;
            resultingVar = resultingEvictionVariant;
            bestEvents = newEvents;
            bestReallyUsedMem = reallyUsedMem;
        }
        else{
            ourModifiedProc.reset();
            for (auto &item: newEvents){
                item.reset();
            }
            for (auto &item: modifiedProcs){
                item.reset();
            }
        }
    }
    //cout << "!!!END BEST"<<endl;
    //cout<<"Best for task "<<vertex->name<<" is on proc "<<bestProcessorToAssign->id<<endl;
    // for (auto &item: bestEvents) {
    //    events.insert(item);
    // }
    // cout << " new events " << endl;
    // cout<< " for "<< vertex->name<<" best "<<bestStartTime<<" "<<bestFinishTime<<" on proc "<<bestProcessorToAssign->id<<endl;//<<" with av mem "<<bestProcessorToAssign->availableMemory<<endl;


     //checkBestEvents(bestEvents);

    vertex->assignedProcessorId = bestProcessorToAssign->id;
    vertex->actuallyUsedMemory = bestReallyUsedMem;
    vertex->status = Status::Scheduled;

    buildPendingMemoriesAfter(bestProcessorToAssign, vertex);

    for (auto &item: bestModifiedProcs) {
        auto iterator = cluster->getProcessors().find(item->id);
        iterator->second->updateFrom(*item);
        assert(iterator->second->getPendingMemories().size() == item->getPendingMemories().size());
    }

    for (int j = 0; j < vertex->in_degree; j++) {
        assert(//vertex->in_edges[j]->tail->makespan==-1 ||
                bestFinishTime > vertex->in_edges[j]->tail->makespan);
    }


    return bestEvents;
}

void checkBestEvents(vector<shared_ptr<Event>> &bestEvents) {
    for (auto &item: bestEvents) {
        cout << item->id << " at " << item->getExpectedTimeFire() << ", " << endl;
        assert(item->getExpectedTimeFire()== item->getActualTimeFire());
        bool hasCycle = item->checkCycleFromEvent();
        assert(!hasCycle);

        if (item->id.find("-w-f") != std::string::npos) {
            auto itWriteStart = std::find_if(bestEvents.begin(), bestEvents.end(),
                                             [item](shared_ptr<Event> e) { return e->id == item->id.substr(0,
                                                                                                           item->id.length() -
                                                                                                           4) + "-w-s";
                                             });
            if (itWriteStart != bestEvents.end()) {
                auto actualLength = item->getExpectedTimeFire() - (*itWriteStart)->getExpectedTimeFire();
                assert(abs(actualLength - item->edge->weight / item->processor->writeSpeedDisk) < 0.00001);
            } else {
                throw runtime_error("no pair found ofr " + item->id);
            }

        }
        if (item->id.find("-r-f") != std::string::npos) {
            auto itReadStart = std::find_if(bestEvents.begin(), bestEvents.end(),
                                            [item](shared_ptr<Event> e) { return e->id == item->id.substr(0,
                                                                                                          item->id.length() -
                                                                                                          4) + "-r-s";
                                            });
            if (itReadStart != bestEvents.end()) {
                auto actualLength = item->getExpectedTimeFire() - (*itReadStart)->getExpectedTimeFire();
                assert(abs(actualLength - item->edge->weight / item->processor->readSpeedDisk) < 0.00001);
                cout << "ys" << endl;
            } else {
                throw runtime_error("no pair found ofr " + item->id);
            }
        }
    }
    cout << endl;
}


vector<shared_ptr<Processor>>
tentativeAssignment(vertex_t *vertex, shared_ptr<Processor> ourModifiedProc,
                    double &finTime, double &startTime, int &resultingVar, vector<shared_ptr<Event>> &newEvents,
                    double &actuallyUsedMemory, double notEarlierTHan) {
    // cout << "try " << ourModifiedProc->id << " for " << vertex->name << endl;
    assert(ourModifiedProc->getAvailableMemory() <= ourModifiedProc->getMemorySize());

    vector<std::shared_ptr<Processor>> modifiedProcs;
    modifiedProcs.emplace_back(ourModifiedProc);

    transferAfterMemoriesToBefore(ourModifiedProc);

    startTime = ourModifiedProc->getReadyTimeCompute();

    vector<shared_ptr<Event>> preds = vector<shared_ptr<Event>>{};
    vector<shared_ptr<Event>> succs = vector<shared_ptr<Event>>{};
    auto eventStartTask = Event::createEvent(vertex, nullptr, OnTaskStart, ourModifiedProc,
                                             startTime, startTime, preds,
                                             succs, false,
                                             vertex->name + "-s");
    auto finishTime = startTime + vertex->time / ourModifiedProc->getProcessorSpeed();
    auto eventFinishTask = Event::createEvent(vertex, nullptr, OnTaskFinish, ourModifiedProc,
                                              finishTime, finishTime, preds,
                                              succs, false,
                                              vertex->name + "-f");
    eventFinishTask->addPredecessor(eventStartTask);

    double sumOut = getSumOut(vertex);
    assert(sumOut == outMemoryRequirement(vertex));
    if (ourModifiedProc->getMemorySize() < outMemoryRequirement(vertex) ||
        ourModifiedProc->getMemorySize() < inMemoryRequirement(vertex)) {
        //   cout<<"too large outs or ins absolutely for tast "<<vertex->name<<endl;
        finTime = std::numeric_limits<double>::max();
        return {};
    }
    realSurplusOfOutgoingEdges(vertex, ourModifiedProc, sumOut);
    double biggestFileWeight = 0;
    double sumWeightsOfAllPending = 0;
    double amountToOffloadWithoutBiggestFile = 0;
    double amountToOffloadWithoutAllFiles = 0;
    double Res = howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(vertex, ourModifiedProc);
    if (Res < 0) {

        double amountToOffload = -Res;
        double shortestFT = std::numeric_limits<double>::max();

        double timeToFinishNoEvicted = startTime + vertex->time / ourModifiedProc->getProcessorSpeed() +
                                       amountToOffload / ourModifiedProc->memoryOffloadingPenalty;
        assert(timeToFinishNoEvicted > startTime);
        if (sumOut > ourModifiedProc->getAvailableMemory()) {
            //cout<<"cant"<<endl;
            timeToFinishNoEvicted = std::numeric_limits<double>::max();
        }
        if (getSumIn(vertex) > ourModifiedProc->getAvailableMemory()) {
            timeToFinishNoEvicted = std::numeric_limits<double>::max();
        }


        double timeToFinishBiggestEvicted = std::numeric_limits<double>::max(),
                timeToFinishAllEvicted = std::numeric_limits<double>::max();
        double timeToWriteAllPending = 0;

        double startTimeFor1Evicted, startTimeForAllEvicted;
        startTimeFor1Evicted = startTimeForAllEvicted = ourModifiedProc->getReadyTimeWrite() > startTime ?
                                                        ourModifiedProc->getReadyTimeWrite() : startTime;
        //TODO what if biggest is incoming?
        edge_t *biggestPendingEdge = *ourModifiedProc->getPendingMemories().begin();
        if (!ourModifiedProc->getPendingMemories().empty()) {
            assert(biggestPendingEdge->weight >=
                   (*ourModifiedProc->getPendingMemories().rbegin())->weight);
            //  cout << "case 2, before " << buildEdgeName(biggestPendingEdge);
            biggestPendingEdge = ourModifiedProc->getBiggestPendingEdgeThatIsNotIncomingOf(vertex);
            if (biggestPendingEdge == nullptr) {
                //  cout << "No pending memories that are not incoming edges of task vertex, no sense in evicting anyone"
                //      << endl;

            } else {
                //      cout << " after " << buildEdgeName(biggestPendingEdge) << endl;
                biggestFileWeight = biggestPendingEdge->weight;
                amountToOffloadWithoutBiggestFile =
                        (amountToOffload - biggestFileWeight) > 0 ? (amountToOffload -
                                                                     biggestFileWeight)
                                                                  : 0;
                double finishTimeToWrite = ourModifiedProc->getReadyTimeWrite() +
                                           biggestFileWeight / ourModifiedProc->writeSpeedDisk;
                startTimeFor1Evicted = max(startTime, finishTimeToWrite);
                timeToFinishBiggestEvicted =
                        startTimeFor1Evicted
                        + vertex->time / ourModifiedProc->getProcessorSpeed() +
                        amountToOffloadWithoutBiggestFile / ourModifiedProc->memoryOffloadingPenalty;
                assert(timeToFinishBiggestEvicted > startTimeFor1Evicted);

                double availableMemWithoutBiggest = ourModifiedProc->getAvailableMemory() + biggestFileWeight;
                if (sumOut > availableMemWithoutBiggest)
                    timeToFinishBiggestEvicted = std::numeric_limits<double>::max();


                sumWeightsOfAllPending = 0;
                for (const auto &item: ourModifiedProc->getPendingMemories()) {
                    if (item->head->name == vertex->name) {
                        //our incoming edge
                        //     cout << "our incoming " << buildEdgeName(item) << endl;
                    } else {
                        timeToWriteAllPending += item->weight / ourModifiedProc->writeSpeedDisk;
                        sumWeightsOfAllPending += item->weight;
                    }

                }

                amountToOffloadWithoutAllFiles = (amountToOffload - sumWeightsOfAllPending > 0) ?
                                                 amountToOffload - sumWeightsOfAllPending : 0;

                assert(amountToOffloadWithoutAllFiles >= 0);
                finishTimeToWrite = ourModifiedProc->getReadyTimeWrite() +
                                    timeToWriteAllPending;
                startTimeForAllEvicted = max(startTimeForAllEvicted, finishTimeToWrite);
                timeToFinishAllEvicted = startTimeForAllEvicted + vertex->time / ourModifiedProc->getProcessorSpeed() +
                                         amountToOffloadWithoutAllFiles / ourModifiedProc->memoryOffloadingPenalty;
                assert(timeToFinishAllEvicted > startTimeForAllEvicted);
            }

        }

        double minTTF = min(timeToFinishNoEvicted, min(timeToFinishBiggestEvicted, timeToFinishAllEvicted));
        if (minTTF == std::numeric_limits<double>::max()) {
            // cout << "minTTF inf" << endl;
            finTime = std::numeric_limits<double>::max();
            return {};
        }

        // ourModifiedProc->readyTimeCompute = minTTF;
        // finTime = ourModifiedProc->readyTimeCompute;
        //assert(ourModifiedProc->readyTimeCompute < std::numeric_limits<double>::max());



        if (timeToFinishNoEvicted == minTTF) {
            resultingVar = 1;
        } else if (timeToFinishBiggestEvicted == minTTF) {
            std::pair<shared_ptr<Event>, shared_ptr<Event>> writeEvents;
            auto it = scheduleWriteForEdge(ourModifiedProc, biggestPendingEdge, writeEvents);
            newEvents.emplace_back(writeEvents.first);
            newEvents.emplace_back(writeEvents.second);
            resultingVar = 2;
            assert(startTime <= startTimeFor1Evicted);
            startTime = startTimeFor1Evicted;

            assert(biggestPendingEdge != nullptr);
            assert(biggestPendingEdge->tail->status == Status::Scheduled ||
                   biggestPendingEdge->tail->status == Status::Running ||
                   isLocatedOnThisProcessor(biggestPendingEdge, ourModifiedProc->id));
        } else if (timeToFinishAllEvicted == minTTF) {
            //  cout<<"case 3 avail mem "<<ourModifiedProc->getAvailableMemory()<<" "<<ourModifiedProc->getAfterAvailableMemory()<<endl;
            resultingVar = 3;
            double initTimeWRite = ourModifiedProc->getReadyTimeWrite();
            std::set<edge_t *, std::function<bool(edge_t *, edge_t *)>> temp(Processor::comparePendingMemories);
            while (!ourModifiedProc->getPendingMemories().empty()) {
                edge_t *edge = (*ourModifiedProc->getPendingMemories().begin());
                //    cout << buildEdgeName(edge) << endl;
                if (edge->head->name == vertex->name) {
                    temp.insert(edge);
                    ourModifiedProc->removePendingMemory(edge);
                } else {
                    std::pair<shared_ptr<Event>, shared_ptr<Event>> writeEvents;
                    scheduleWriteForEdge(ourModifiedProc, edge, writeEvents);
                    newEvents.emplace_back(writeEvents.first);
                    newEvents.emplace_back(writeEvents.second);
                }
            }
            for (auto &temEd: temp) {
                ourModifiedProc->addPendingMemory(temEd);
            }

            assert(startTime <= startTimeForAllEvicted);
            startTime = startTimeForAllEvicted;

        }

        processIncomingEdges(vertex, eventStartTask, ourModifiedProc, modifiedProcs, newEvents);

        if (timeToFinishNoEvicted == minTTF) {
            assert(ourModifiedProc->getAvailableMemory() <= ourModifiedProc->getMemorySize());
            actuallyUsedMemory = ourModifiedProc->getAvailableMemory();
            ourModifiedProc->setAvailableMemory(0);
        } else if (timeToFinishBiggestEvicted == minTTF) {
            actuallyUsedMemory = min(ourModifiedProc->getAvailableMemory(), peakMemoryRequirementOfVertex(vertex));
            assert(actuallyUsedMemory <= ourModifiedProc->getMemorySize());
            ourModifiedProc->setAvailableMemory(max(0.0,
                                                    ourModifiedProc->getAvailableMemory() -
                                                    vertex->memoryRequirement
                                                )
            );
        } else if (timeToFinishAllEvicted == minTTF) {
            actuallyUsedMemory = min(ourModifiedProc->getMemorySize(), peakMemoryRequirementOfVertex(vertex));
            ourModifiedProc->setAvailableMemory(max(0.0,
                                                    ourModifiedProc->getMemorySize() -
                                                    peakMemoryRequirementOfVertex(vertex)
                                                )
            );
            //  cout<<"case 3 end avail mem "<<ourModifiedProc->getAvailableMemory()<<" "<<ourModifiedProc->getAfterAvailableMemory()<<endl;
        }

    } else {
        processIncomingEdges(vertex, eventStartTask, ourModifiedProc, modifiedProcs, newEvents);

        actuallyUsedMemory = peakMemoryRequirementOfVertex(vertex);
        assert(actuallyUsedMemory <= ourModifiedProc->getMemorySize());
        //  ourModifiedProc->setAvailableMemory(
        //         ourModifiedProc->getAvailableMemory() - peakMemoryRequirementOfVertex(vertex));
    }

    startTime = max(max(max(ourModifiedProc->getReadyTimeCompute(), ourModifiedProc->getReadyTimeRead()), startTime), eventStartTask->getExpectedTimeFire());

    eventStartTask->setBothTimesFire(startTime);



    for (auto &newEvent: newEvents) {
        if (newEvent->id.find("-f") != std::string::npos) {
            eventStartTask->addPredecessor(newEvent);
        }
    }

    finishTime = eventStartTask->getExpectedTimeFire() + vertex->time / ourModifiedProc->getProcessorSpeed();
    if (resultingVar == 1) {
        assert(Res < 0);
        finishTime += abs(Res) / ourModifiedProc->memoryOffloadingPenalty;
    } else if (resultingVar == 2) {
        finishTime += amountToOffloadWithoutBiggestFile / ourModifiedProc->memoryOffloadingPenalty;
    } else if (resultingVar == 3) {
        finishTime += amountToOffloadWithoutAllFiles / ourModifiedProc->memoryOffloadingPenalty;
    }
    if (vertex->time == 0) {
        finishTime = finishTime + 0.0001;
    }

    eventFinishTask->setBothTimesFire(finishTime);

    if(eventStartTask->getExpectedTimeFire() >= eventFinishTask->getExpectedTimeFire()){
        cout<<" BAD START/FINSH TIME TASK "<< eventStartTask->getExpectedTimeFire()<<" "<< eventFinishTask->getExpectedTimeFire()
                                                                                        <<"FOR TASK "<<vertex->name<<
                                                                                        " vertex time  "<< vertex->time<<
        endl;

    }
    assert(vertex->time / ourModifiedProc->getProcessorSpeed() < 0.001 || eventStartTask->getExpectedTimeFire() < eventFinishTask->getExpectedTimeFire());
    assert(eventStartTask->getExpectedTimeFire()== eventStartTask->getActualTimeFire());

    newEvents.emplace_back(eventStartTask);
    newEvents.emplace_back(eventFinishTask);

    ourModifiedProc->addEvent(eventStartTask);
    ourModifiedProc->addEvent(eventFinishTask);

    if(eventStartTask->getExpectedTimeFire() >= eventFinishTask->getExpectedTimeFire()){
        cout<<" BAD START/FINSH TIME TASK "<< eventStartTask->getExpectedTimeFire()<<" "<< eventFinishTask->getExpectedTimeFire()
            <<"FOR TASK "<<vertex->name<<
            " vertex time  "<< vertex->time<< " duarion "<<vertex->time / ourModifiedProc->getProcessorSpeed()<<
            endl;

    }
    assert(vertex->time / ourModifiedProc->getProcessorSpeed() < 0.001 || eventStartTask->getExpectedTimeFire() < eventFinishTask->getExpectedTimeFire());

    eventFinishTask->addPredecessor(eventStartTask);
    if (!ourModifiedProc->getLastComputeEvent().expired() && !ourModifiedProc->getLastComputeEvent().lock()->isDone) {
        eventStartTask->addPredecessor(ourModifiedProc->getLastComputeEvent().lock());
    }
    //  cout << "SET LAST COMPUTE EVENT TO " << eventFinishTask->id << endl;
    ourModifiedProc->setLastComputeEvent(eventFinishTask);
    finTime = ourModifiedProc->getReadyTimeCompute();

    return modifiedProcs;
}


double
processIncomingEdges(const vertex_t *v, shared_ptr<Event> &ourEvent, shared_ptr<Processor> &ourModifiedProc,
                     vector<std::shared_ptr<Processor>> &modifiedProcs,
                     vector<shared_ptr<Event>> &createdEvents) {
    //cout<<"processing, avail mem "<<ourModifiedProc->getAvailableMemory()<<endl;

    double howMuchWasLoaded = ourModifiedProc->getAvailableMemory();
    int ind = v->in_degree;
    // if(ind>0){
    for (int p = 0; p < ind; p++) {
        edge *incomingEdge = v->in_edges[p];
        vertex_t *predecessor = incomingEdge->tail;
        if(predecessor->makespan>0){
            ourEvent->setBothTimesFire(max(ourEvent->getExpectedTimeFire(), predecessor->makespan));
        }
        if (ourModifiedProc->getPendingMemories().find(incomingEdge) != ourModifiedProc->getPendingMemories().end()) {
            //    cout<<"already on proc, judginbg from proc"<<endl;
        } else if (isLocatedNowhere(incomingEdge)) {
            shared_ptr<Processor> plannedOnThisProc = nullptr;
            for (const auto &item: cluster->getProcessors()) {
                if (item.first != ourModifiedProc->id) {
                    if (item.second->getPendingMemories().find(incomingEdge) !=
                        item.second->getPendingMemories().end()) {
                        //     cout<<"found planned in pending mems on proc "<<item.second->id<<endl;
                        plannedOnThisProc = item.second;
                        break;
                    }
                    if (item.second->getAfterPendingMemories().find(incomingEdge) !=
                        item.second->getAfterPendingMemories().end()) {
                        //        cout<<"found planned in after pending mems on proc "<<item.second->id<<endl;
                        plannedOnThisProc = item.second;
                        break;
                    }
                }
            }
            if (plannedOnThisProc == nullptr) {
                //  throw runtime_error("Edge located nowhere " + buildEdgeName(incomingEdge));
                //it has been written to disk, but not yet fired the event
                auto readEvents = scheduleARead(v, ourEvent, createdEvents, ourEvent->getExpectedTimeFire(), ourModifiedProc,
                                                incomingEdge);
                const shared_ptr<Event> &eventFinishThisEdgeWrite = events.find(buildEdgeName(incomingEdge) + "-w-f");
                if (eventFinishThisEdgeWrite != nullptr) {
                    double eventFinishThisEdgeWritebef = eventFinishThisEdgeWrite->getActualTimeFire();
                    readEvents.first->addPredecessor(eventFinishThisEdgeWrite);
                    assert(eventFinishThisEdgeWritebef == eventFinishThisEdgeWrite->getActualTimeFire());
                } else {
                    if (isLocatedOnDisk(incomingEdge) || incomingEdge
                                                                 ->tail->name == "GRAPH_SOURCE") {
                    } else {
                        cout << "no event finish write - AND THE FILE IS NOT ON DISK " << buildEdgeName(incomingEdge)
                             << endl;
                    }
                }

            } else {
                auto predProc = findPredecessorsProcessor(incomingEdge, modifiedProcs);
                assert(predProc->id == plannedOnThisProc->id);
                scheduleWriteAndRead(v, ourEvent, createdEvents, ourEvent->getExpectedTimeFire(), ourModifiedProc, incomingEdge,
                                     modifiedProcs);
            }

        } else if (isLocatedOnThisProcessor(incomingEdge, ourModifiedProc->id)) {
            //   cout << "edge " << buildEdgeName(incomingEdge) << " already on proc" << endl;
        } else if (isLocatedOnDisk(incomingEdge)) {

            // schedule a read
            scheduleARead(v, ourEvent, createdEvents, ourEvent->getExpectedTimeFire(), ourModifiedProc, incomingEdge);
        } else if (isLocatedOnAnyProcessor(incomingEdge)) {
            shared_ptr<Processor> predecessorsProc = findPredecessorsProcessor(incomingEdge, modifiedProcs);
            if (predecessorsProc->getAfterPendingMemories().find(incomingEdge) ==
                predecessorsProc->getAfterPendingMemories().end()) {
                //    cout << "edge " << buildEdgeName(incomingEdge) << " not found in after pending mems on proc "
                //        << predecessorsProc->id << endl;
                auto plannedWriteFinishOfIncomingEdge = events.find(buildEdgeName(incomingEdge) + "-w-f");
                assert(plannedWriteFinishOfIncomingEdge != nullptr);
                std::pair<shared_ptr<Event>, shared_ptr<Event>> readEVents;
                double prev = plannedWriteFinishOfIncomingEdge->getActualTimeFire();
                if (plannedWriteFinishOfIncomingEdge->getActualTimeFire() > ourEvent->getExpectedTimeFire() &&
                    plannedWriteFinishOfIncomingEdge->getActualTimeFire() > ourModifiedProc->getReadyTimeRead()) {

                    readEVents = scheduleARead(v, ourEvent, createdEvents, ourEvent->getExpectedTimeFire(), ourModifiedProc,
                                               incomingEdge, plannedWriteFinishOfIncomingEdge->getActualTimeFire());
                    readEVents.first->addPredecessor(plannedWriteFinishOfIncomingEdge);

                } else {
                    readEVents = scheduleARead(v, ourEvent, createdEvents, ourEvent->getExpectedTimeFire(), ourModifiedProc,
                                               incomingEdge);
                    readEVents.first->addPredecessor(plannedWriteFinishOfIncomingEdge);
                }
                assert(prev== plannedWriteFinishOfIncomingEdge->getActualTimeFire());
                assert(readEVents.first->getActualTimeFire() < readEVents.second->getActualTimeFire());
            } else {
                //schedule a write
                scheduleWriteAndRead(v, ourEvent, createdEvents, ourEvent->getExpectedTimeFire(), ourModifiedProc, incomingEdge,
                                     modifiedProcs);
            }
        }
    }
    //}
    howMuchWasLoaded = howMuchWasLoaded - ourModifiedProc->getAvailableMemory();
    assert(howMuchWasLoaded >= 0);
    //   cout<<"loaded all incoming, avail mem "<<ourModifiedProc->getAvailableMemory()<<endl;
    ourModifiedProc->setAvailableMemory(ourModifiedProc->getAvailableMemory() + howMuchWasLoaded);
    //  cout<<"finally, avail mem "<<ourModifiedProc->getAvailableMemory()<<endl;
    return 0;
}

std::pair<shared_ptr<Event>, shared_ptr<Event>>
scheduleARead(const vertex_t *v, shared_ptr<Event> &ourEvent, vector<shared_ptr<Event>> &createdEvents,
              double startTimeOfTask,
              shared_ptr<Processor> &ourModifiedProc, edge *&incomingEdge, double atThisTime) {

    double estimatedStartOfRead = startTimeOfTask - incomingEdge->weight / ourModifiedProc->readSpeedDisk;
    estimatedStartOfRead = max(estimatedStartOfRead, ourModifiedProc->getReadyTimeRead());
    vector<shared_ptr<Event>> predsOfRead = vector<shared_ptr<Event>>{};
    vector<shared_ptr<Event>> succsOfRead = vector<shared_ptr<Event>>{};
    if (events.find(v->name + "-s") != nullptr) {
        succsOfRead.emplace_back(events.find(v->name + "-s"));
    }

    //if this start of the read is happening during the runtime  of the previous task
//TODO WHAT IF BEFORE IT STARTS?
    assert(ourModifiedProc->getLastComputeEvent().expired() ||
           abs(ourModifiedProc->getReadyTimeCompute() -
               ourModifiedProc->getLastComputeEvent().lock()->getActualTimeFire()) < 0.001);
    if (estimatedStartOfRead < ourModifiedProc->getReadyTimeCompute() &&
        ourModifiedProc->getAvailableMemory() < incomingEdge->weight) {
        estimatedStartOfRead = ourModifiedProc->getReadyTimeCompute();
    }

    if (atThisTime != -1) {
        estimatedStartOfRead = atThisTime;
    }

    vector<shared_ptr<Event>> newEvents = evictFilesUntilThisFits(ourModifiedProc, incomingEdge->weight);
    if (!newEvents.empty()) {
        // cout << "evicted" << newEvents.size() << endl;

    }
    createdEvents.insert(createdEvents.end(), newEvents.begin(), newEvents.end());

    //??????If the starting time of the read is during the execution of the previous task and there
    // is enough memory to read this file, then we move the read forward to the beginning of this previous task.
    auto eventStartRead = Event::createEvent(nullptr, incomingEdge, OnReadStart, ourModifiedProc,
                                             estimatedStartOfRead, estimatedStartOfRead, predsOfRead,
                                             succsOfRead, false,
                                             buildEdgeName(incomingEdge) + "-r-s");
    createdEvents.emplace_back(eventStartRead);
    if (!ourModifiedProc->getLastReadEvent().expired() && !ourModifiedProc->getLastReadEvent().lock()->isDone) {
        double prev = ourModifiedProc->getLastReadEvent().lock()->getActualTimeFire();
        eventStartRead->addPredecessor(ourModifiedProc->getLastReadEvent().lock());
        assert(prev==ourModifiedProc->getLastReadEvent().lock()->getActualTimeFire());
    }

    const shared_ptr<Event> &eventFinishPredecessorComputing = events.find(incomingEdge->tail->name + "-f");
    if (eventFinishPredecessorComputing != nullptr) {
        double prev = eventFinishPredecessorComputing->getActualTimeFire();
        eventStartRead->addPredecessor(eventFinishPredecessorComputing);
        assert(prev==eventFinishPredecessorComputing->getActualTimeFire());
    } else {
        if (incomingEdge->tail->status == Status::Finished) {
            if (eventStartRead->getExpectedTimeFire() < incomingEdge->tail->makespan) {
                double diff = incomingEdge->tail->makespan - eventStartRead->getExpectedTimeFire();
                eventStartRead->setBothTimesFire(incomingEdge->tail->makespan);
                if (!eventStartRead->successors.empty()) {
                    eventStartRead->propagateChain(eventStartRead, diff);
                }
            }
        } else {
            cout << "no event finish predecessor - AND THE TAIL IS NOT FINISHED" << endl;
        }
    }


    double estimatedTimeOfFinishRead =
            eventStartRead->getExpectedTimeFire() + incomingEdge->weight / ourModifiedProc->readSpeedDisk;

    predsOfRead = vector<shared_ptr<Event>>{};
    succsOfRead = vector<shared_ptr<Event>>{};
    auto eventFinishRead = Event::createEvent(nullptr, incomingEdge, OnReadFinish, ourModifiedProc,
                                              estimatedTimeOfFinishRead, estimatedTimeOfFinishRead, predsOfRead,
                                              succsOfRead, false,
                                              buildEdgeName(incomingEdge) + "-r-f");

    eventFinishRead->addSuccessor(ourEvent);
    if (events.find(v->name + "-s") != nullptr) {
        eventFinishRead->addSuccessor(events.find(v->name + "-s"));
    }
    eventFinishRead->addPredecessor(eventStartRead);

    createdEvents.emplace_back(eventFinishRead);

    //ourModifiedProc->readyTimeRead = estimatedTimeOfFinishRead;
    ourModifiedProc->setLastReadEvent(eventFinishRead);
    ourModifiedProc->addPendingMemory(incomingEdge);
    assert(eventFinishRead->getActualTimeFire() == eventFinishRead->getExpectedTimeFire());
    if(incomingEdge->weight / ourModifiedProc->readSpeedDisk> 0.001){
        if(eventFinishRead->getExpectedTimeFire() <= eventStartRead->getExpectedTimeFire()){
            cout<<"BAD TIMES FINISH AND START READ FOR "<<buildEdgeName(incomingEdge)<< " FINISH AT "<<eventFinishRead->getExpectedTimeFire()
                <<" START AT "<<eventStartRead->getExpectedTimeFire()<<" planned finish  at "<<estimatedTimeOfFinishRead<<" duration of edge "<<incomingEdge->weight / ourModifiedProc->readSpeedDisk<<endl;
            cout<<"was finihs moved? "<< (eventFinishRead->getExpectedTimeFire()==estimatedTimeOfFinishRead? "no": "yes")<<endl;
        }
        assert(eventFinishRead->getExpectedTimeFire() > eventStartRead->getExpectedTimeFire());
    }
    auto actualLength = eventFinishRead->getExpectedTimeFire() - eventStartRead->getExpectedTimeFire();
   // if(abs(actualLength - incomingEdge->weight / ourModifiedProc->readSpeedDisk) > 0.00001){
        //cerr<<"WRONG LENGTH OF READ PLANNED ON "<<buildEdgeName(incomingEdge)<<" actual length "<<actualLength<<" should be "<<to_string(incomingEdge->weight / ourModifiedProc->readSpeedDisk)<<endl;
    //}
   assert(abs(actualLength - incomingEdge->weight / ourModifiedProc->readSpeedDisk) < 0.001);
    // cout<<"reads at "<<eventStartRead->getActualTimeFire()<<" and "<<eventFinishRead->getActualTimeFire()<<endl;
    return {eventStartRead, eventFinishRead};
}


vector<shared_ptr<Event>> evictFilesUntilThisFits(shared_ptr<Processor> thisProc, double weightToFit) {
    vector<shared_ptr<Event>> newEvents;
    auto begin = thisProc->getPendingMemories().begin();
    while (begin != thisProc->getPendingMemories().end()) {
        begin++;
    }
    while (thisProc->getAvailableMemory() < weightToFit && !thisProc->getPendingMemories().empty()) {
        edge_t *edgeToEvict = *thisProc->getPendingMemories().begin();
        std::pair<shared_ptr<Event>, shared_ptr<Event>> writeEvents;
        auto iterator = scheduleWriteForEdge(thisProc, edgeToEvict, writeEvents);
        newEvents.emplace_back(writeEvents.first);
        newEvents.emplace_back(writeEvents.second);
    }
    return newEvents;
}

//std::pair<shared_ptr<Event>, shared_ptr<Event>>
set<edge_t *, bool (*)(edge_t *, edge_t *)>::iterator
scheduleWriteForEdge(shared_ptr<Processor> &thisProc, edge_t *edgeToEvict,
                     std::pair<shared_ptr<Event>, shared_ptr<Event>> &writeEvents) {
    //cout << "schedule write for edge evicting " << buildEdgeName(edgeToEvict) << endl;

    vector<shared_ptr<Event>> pred, succ = vector<shared_ptr<Event>>{};
    auto eventStartWrite = Event::createEvent(nullptr, edgeToEvict, OnWriteStart, thisProc,
                                              thisProc->getReadyTimeWrite(), thisProc->getReadyTimeWrite(),
                                              pred,
                                              succ, false,
                                              buildEdgeName(edgeToEvict) + "-w-s");


    if (!thisProc->getLastWriteEvent().expired() && !thisProc->getLastWriteEvent().lock()->isDone) {
        double prev = thisProc->getLastWriteEvent().lock()->getActualTimeFire();
        eventStartWrite->addPredecessor(thisProc->getLastWriteEvent().lock());
        thisProc->getLastWriteEvent().lock()->addSuccessor(eventStartWrite);
        assert(prev== thisProc->getLastWriteEvent().lock()->getActualTimeFire());
    }

    auto eventOfFinishPredecessor = events.find(edgeToEvict->tail->name + "-f");
    if (eventOfFinishPredecessor == nullptr) {
        //  cout << " no event of finish prdecesor found for edge " << buildEdgeName(edgeToEvict) << endl;
    } else {
        // cout << " event of finish prdecesor FOUND for edge " << buildEdgeName(edgeToEvict) << endl;
        // cout << "it is " << eventOfFinishPredecessor->id << " at " << eventOfFinishPredecessor->actualTimeFire << endl;
        if (!eventOfFinishPredecessor->isDone) {
            eventStartWrite->addPredecessor(eventOfFinishPredecessor);
        }

    }

    double timeFinishWrite = eventStartWrite->getExpectedTimeFire() + edgeToEvict->weight / thisProc->writeSpeedDisk;

    auto eventFinishWrite = Event::createEvent(nullptr, edgeToEvict, OnWriteFinish, thisProc,
                                               timeFinishWrite,
                                               timeFinishWrite,
                                               pred,
                                               succ, false,
                                               buildEdgeName(edgeToEvict) + "-w-f");

    eventFinishWrite->addPredecessor(eventStartWrite);
    thisProc->setLastWriteEvent(eventFinishWrite);

    assert(thisProc->getAvailableMemory() >= 0);
    writeEvents.first = eventStartWrite;
    writeEvents.second = eventFinishWrite;

    assert(eventStartWrite->getActualTimeFire() == eventStartWrite->getExpectedTimeFire());
    assert(eventFinishWrite->getExpectedTimeFire() ==
           eventStartWrite->getExpectedTimeFire() + edgeToEvict->weight / thisProc->writeSpeedDisk);
    //cout<<"write "<<buildEdgeName(edgeToEvict)<<" start at "<<eventStartWrite->getActualTimeFire()<<" finish at "<<eventFinishWrite->getActualTimeFire()<< " on proc "<<thisProc->id<<endl;

    return thisProc->removePendingMemory(edgeToEvict);

}

void scheduleWriteAndRead(const vertex_t *v, shared_ptr<Event> ourEvent, vector<shared_ptr<Event>> &createdEvents,
                          double startTimeOfTask,
                          shared_ptr<Processor> &ourModifiedProc, edge *&incomingEdge,
                          vector<std::shared_ptr<Processor>> &modifiedProcs) {
    //cout << "scehdule write and read for " << buildEdgeName(incomingEdge) << endl;

    shared_ptr<Processor> predecessorsProc = findPredecessorsProcessor(incomingEdge, modifiedProcs);
    double estimatedStartOfRead = startTimeOfTask - incomingEdge->weight / ourModifiedProc->readSpeedDisk;
    estimatedStartOfRead = max(estimatedStartOfRead, ourModifiedProc->getReadyTimeRead());

    assert(ourModifiedProc->getLastComputeEvent().expired() ||
           abs(ourModifiedProc->getReadyTimeCompute() -
               ourModifiedProc->getLastComputeEvent().lock()->getActualTimeFire()) < 0.001);
    if (estimatedStartOfRead < ourModifiedProc->getReadyTimeCompute() &&
        ourModifiedProc->getAvailableMemory() < incomingEdge->weight) {
        estimatedStartOfRead = ourModifiedProc->getReadyTimeCompute();
    }

    double estimatedStartOfWrite = estimatedStartOfRead - incomingEdge->weight / predecessorsProc->writeSpeedDisk;
    estimatedStartOfWrite = max(estimatedStartOfWrite, predecessorsProc->getReadyTimeWrite());

    if (events.find(incomingEdge->tail->name + "-f") != nullptr) {
        estimatedStartOfWrite = max(estimatedStartOfWrite,
                                    events.find(incomingEdge->tail->name + "-f")->getActualTimeFire());
    } else {
        assert(incomingEdge->tail->makespan != -1);
        estimatedStartOfWrite = max(estimatedStartOfWrite, incomingEdge->tail->makespan);
    }

    estimatedStartOfRead = estimatedStartOfWrite + incomingEdge->weight / predecessorsProc->writeSpeedDisk;
    double estimatedTimeOfReadyRead = estimatedStartOfRead + incomingEdge->weight / ourModifiedProc->readSpeedDisk;

    pair<shared_ptr<Event>, shared_ptr<Event>> readEvents = scheduleARead(v, ourEvent, createdEvents,
                                                                          startTimeOfTask, ourModifiedProc,
                                                                          incomingEdge, estimatedStartOfRead);
    double slack = 0;
    if (readEvents.second->getActualTimeFire() > estimatedTimeOfReadyRead) {
        slack = readEvents.second->getExpectedTimeFire() - estimatedTimeOfReadyRead;
        estimatedStartOfWrite = estimatedStartOfWrite + slack;
    }

    vector<shared_ptr<Event>> predsOfWrite = vector<shared_ptr<Event>>{};
    vector<shared_ptr<Event>> succsOfWrite = vector<shared_ptr<Event>>{};

    auto eventStartWrite = Event::createEvent(nullptr, incomingEdge, OnWriteStart, predecessorsProc,
                                              estimatedStartOfWrite, estimatedStartOfWrite, predsOfWrite,
                                              succsOfWrite, false,
                                              buildEdgeName(incomingEdge) + "-w-s");
    assert(eventStartWrite->getExpectedTimeFire() == estimatedStartOfWrite);
    assert(eventStartWrite->getExpectedTimeFire() == eventStartWrite->getActualTimeFire());

    if (!predecessorsProc->getLastWriteEvent().expired() && !predecessorsProc->getLastWriteEvent().lock()->isDone) {
        eventStartWrite->addPredecessor(predecessorsProc->getLastWriteEvent().lock());
    }


    if (events.find(incomingEdge->tail->name + "-f") != nullptr) {
        double prev = events.find(incomingEdge->tail->name + "-f")->getActualTimeFire();
        eventStartWrite->addPredecessor(events.find(incomingEdge->tail->name + "-f"));
        assert(prev== events.find(incomingEdge->tail->name + "-f")->getActualTimeFire());
    }

    createdEvents.emplace_back(eventStartWrite);

    double estimatedTimeOfFinishWrite =
            eventStartWrite->getExpectedTimeFire() + incomingEdge->weight / predecessorsProc->writeSpeedDisk;

    predsOfWrite = vector<shared_ptr<Event>>{};
    succsOfWrite = vector<shared_ptr<Event>>{};
    auto eventFinishWrite = Event::createEvent(nullptr, incomingEdge, OnWriteFinish, predecessorsProc,
                                               estimatedTimeOfFinishWrite, estimatedTimeOfFinishWrite, predsOfWrite,
                                               succsOfWrite, false,
                                               buildEdgeName(incomingEdge) + "-w-f");

    if (events.find(v->name + "-s")) {
        eventFinishWrite->addSuccessor(events.find(v->name + "-s"));
    }
    eventFinishWrite->addPredecessor(eventStartWrite);
    eventFinishWrite->addSuccessor(readEvents.first);

    assert(estimatedTimeOfFinishWrite <= readEvents.first->getExpectedTimeFire());

    assert(incomingEdge->weight < 0.001 || estimatedTimeOfFinishWrite > eventStartWrite->getExpectedTimeFire());

    createdEvents.emplace_back(eventFinishWrite);
    predecessorsProc->setLastWriteEvent(eventFinishWrite);

    predecessorsProc->removePendingMemoryAfter(incomingEdge);
    readEvents.first->addPredecessor(eventFinishWrite);

    // cout << buildEdgeName(incomingEdge)<< " start write at " << eventStartWrite->getActualTimeFire() << " finish at "
    //      << eventFinishWrite->getActualTimeFire() << endl;
    assert( incomingEdge->weight < 0.001 ||  eventStartWrite->getActualTimeFire() < eventFinishWrite->getActualTimeFire());
    assert(eventFinishWrite->getActualTimeFire() ==
           eventStartWrite->getActualTimeFire() + incomingEdge->weight / predecessorsProc->writeSpeedDisk);

}



void buildPendingMemoriesAfter(shared_ptr<Processor> &ourModifiedProc, vertex_t *ourVertex) {
    assert(ourVertex->memoryRequirement == 0 ||
           (ourVertex->actuallyUsedMemory != -1 && ourVertex->actuallyUsedMemory != 0));
    //   cout << "act used " << ourVertex->actuallyUsedMemory << endl;
    // ourModifiedProc->setAfterAvailableMemory(ourModifiedProc->getAvailableMemory() + ourVertex->actuallyUsedMemory);
    ourModifiedProc->setAfterAvailableMemory(ourModifiedProc->getMemorySize());
    bool wasMemWrong = false;
    for (auto &item: ourModifiedProc->getPendingMemories()) {
        try {
            ourModifiedProc->addPendingMemoryAfter(item);
        }
        catch (...) {
            cout << "memor temporaroly wrong!" << endl;
            wasMemWrong = true;
        }
    }
    //  assert(ourModifiedProc->getAfterAvailableMemory() >= 0);
    //cout << "after adding " << endl;
    for (int j = 0; j < ourVertex->in_degree; j++) {
        if (ourModifiedProc->getAfterPendingMemories().find(ourVertex->in_edges[j]) ==
            ourModifiedProc->getAfterPendingMemories().end()) {
            //  cout << "edge " << buildEdgeName(ourVertex->in_edges[j]) << " not found in after pending mems on proc "
            //      << ourModifiedProc->id << endl;
        } else {
            ourModifiedProc->removePendingMemoryAfter(ourVertex->in_edges[j]);
        }
    }
    for (int j = 0; j < ourVertex->out_degree; j++) {
        ourModifiedProc->addPendingMemoryAfter(ourVertex->out_edges[j]);
        assert(ourModifiedProc->getAfterPendingMemories().find(ourVertex->out_edges[j])
               != ourModifiedProc->getPendingMemories().end());
    }
    //ourModifiedProc->setAfterAvailableMemory(
    //       min( ourModifiedProc->getMemorySize(),
    //      ourModifiedProc->getAfterAvailableMemory()+ourVertex->actuallyUsedMemory));

    if (wasMemWrong) {
        assert(ourModifiedProc->getAvailableMemory() >= 0 &&
               ourModifiedProc->getAvailableMemory() < ourModifiedProc->getMemorySize());
        assert(ourModifiedProc->getAfterAvailableMemory() >= 0 &&
               ourModifiedProc->getAfterAvailableMemory() < ourModifiedProc->getMemorySize());
    }
}