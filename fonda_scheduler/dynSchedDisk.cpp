
#include <queue>
#include "fonda_scheduler/dynSchedDisk.hpp"
#include "fonda_scheduler/dynSched.hpp"

Cluster *cluster;
EventManager events;

void checkBestEvents(vector<shared_ptr<Event>> &bestEvents);

double new_heuristic_dynamic(graph_t *graph, Cluster *cluster1, int algoNum, bool isHeft) {
    double  resMakespan=-1;
    cluster = cluster1;
    algoNum = isHeft ? 1 : algoNum;
    enforce_single_source_and_target_with_minimal_weights(graph);
    vertex_t *vertex = graph->first_vertex;
    while (vertex != nullptr) {
        if (vertex->in_degree == 0) {
          //  cout << "starting task " << vertex->name << endl;
            vector<shared_ptr<Processor>> bestModifiedProcs;
            shared_ptr<Processor> bestProcessorToAssign;
            vector<shared_ptr<Event>> newEvents =
                    bestTentativeAssignment(vertex, bestModifiedProcs, bestProcessorToAssign, 0);

            for (auto &item: newEvents) {
                events.insert(item);
                item->processor->addEvent(item);
            }
            vertex->status = Status::Scheduled;
        }
        vertex = vertex->next;
    }
    int cntr = 0;
    while (!events.empty()) {
      //  cout << "NEXT "; //events.printAll();
        cntr++;
        shared_ptr<Event> firstEvent = events.getEarliest();
     //   cout << "event " << firstEvent->id << " at " << firstEvent->getActualTimeFire() << endl;

        //if(firstEvent->id)
        bool removed = events.remove(firstEvent->id);
        assert(removed == true);

        firstEvent->fire();
        resMakespan = max(resMakespan, firstEvent->getActualTimeFire());
        //cout<<"events now "; events.printAll();
    }
    //  cout<<cntr<<" "<<graph->number_of_vertices<<endl;
    //cout << cluster->getMemBiggestFreeProcessor()->getReadyTimeCompute() << endl;

    return resMakespan;
}

void Event::fireTaskStart() {
    string thisid = this->id;
   // cout << "task start for " << thisid<<" at "<<this->actualTimeFire <<" on proc "<<this->processor->id<< endl;
    double timeStart = 0;

    if(this->id=="MAKE_GENOME_FILTER_00000681-s"){
     //   cout<<endl;
    }

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        this->setActualTimeFire(this->getActualTimeFire()+ + std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        events.insert(shared_from_this());
    } else {
        removeOurselfFromSuccessors(this);
        this->task->status = Status::Running;
        auto ourFinishEvent = events.find(this->task->name + "-f");
        if (ourFinishEvent == nullptr) {
            throw runtime_error("NOt found finish event to " + this->task->name);
        }

        double actualRuntime = deviation(ourFinishEvent->getExpectedTimeFire() - this->getExpectedTimeFire());
        assert(fabs(this->getActualTimeFire()- this->getExpectedTimeFire())<0.01);
        double d = this->getActualTimeFire() + actualRuntime;
       // cout << "on start  setting finish time from "<< ourFinishEvent->actualTimeFire <<" to " << d << endl;
        ourFinishEvent->setActualTimeFire( d);
        this->isDone = true;
    }
}


void Event::fireTaskFinish() {
    vertex_t *thisTask = this->task;
 //   cout << "firing task Finish for " << this->id << endl;

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        events.insert(shared_from_this());
    } else {
        removeOurselfFromSuccessors(this);
        //set its status to finished
        this->task->status = Status::Finished;
        this->isDone = true;
        this->task->makespan= this->actualTimeFire;

        assert(this->processor->getAvailableMemory() <= this->processor->getMemorySize());


        string thisId = this->id;
     //   cout << "EVENTS on our processor" << endl;
    //    for (const auto &item: this->processor->getEvents()) {
      //      cout << item.first << "\t";
      //  }
       // cout << endl;

        for (int i = 0; i < thisTask->out_degree; i++) {
            locateToThisProcessorFromNowhere(thisTask->out_edges[i], this->processor->id);
        }

        //Then task goes over its successor tasks in the workflow and schedules ready ones.
        for (int i = 0; i < thisTask->out_degree; i++) {
            vertex_t *childTask = thisTask->out_edges[i]->head;
           // cout << "deal with child " << childTask->name << endl;
            bool isReady = true;
            for (int j = 0; j < childTask->in_degree; j++) {
                if (childTask->in_edges[j]->tail->status == Status::Unscheduled) {
                    isReady = false;
                }
            }
            if (childTask->status == Status::Scheduled) {
          //      cout << "child already scheduled " << childTask->name << endl;
            }

            std::vector<shared_ptr<Event> > pred, succ;
            //TODO Can be two times in queue?
            if (isReady && childTask->status != Status::Scheduled) {
                vector<shared_ptr<Processor>> bestModifiedProcs;
                shared_ptr<Processor> bestProcessorToAssign;
                vector<shared_ptr<Event>> newEvents =
                        bestTentativeAssignment(childTask, bestModifiedProcs, bestProcessorToAssign, this->actualTimeFire);

                for (const auto &item: newEvents) {
                    events.insert(item);
                    item->processor->addEvent(item);
                }
                childTask->status = Status::Scheduled;
            }
        }
        this->isDone = true;
    }
  //  cout << "for " <<//"task finish FINISHED " <<
   // this->task->name<<" best " <<this->actualTimeFire<< " on proc "<<this->processor->id<< endl;

    //events.printAll();
   // cluster->printProcessorsEvents();

}


vector<shared_ptr<Event>> bestTentativeAssignment(vertex_t *vertex, vector<shared_ptr<Processor>> &bestModifiedProcs,
                                                  shared_ptr<Processor> &bestProcessorToAssign, double notEarlierThan) {
    //cout << "!!!START BEST tent assign for " << vertex->name << endl;
    double bestStartTime = numeric_limits<double>::max(), bestFinishTime = numeric_limits<double>::max(),
            bestReallyUsedMem;
    vector<shared_ptr<Event>> bestEvents;
    int resultingVar;
    if(vertex->name=="SORT_BAM_00000011"){
        cout<<endl;
    }

  //  double finishOfLatestParent = notEarlierThan;

    //for (int j = 0; j < vertex->in_degree; j++) {
     //   events.fvertex->in_edges[j]->tail
   // }


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
    }
    //cout << "!!!END BEST"<<endl;
      //cout<<"Best for task "<<vertex->name<<" is on proc "<<bestProcessorToAssign->id<<endl;
    // for (auto &item: bestEvents) {
    //    events.insert(item);
    // }
    // cout << " new events " << endl;
    cout<< " for "<< vertex->name<<" best "<<bestStartTime<<" "<<bestFinishTime<<" on proc "<<bestProcessorToAssign->id<<endl;//<<" with av mem "<<bestProcessorToAssign->availableMemory<<endl;


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

    /* for (auto &item: bestEvents) {
         //assert(item->processor->pendingMemories.size())
         if (item->type == eventType::OnWriteStart) {
             delocateFromThisProcessorToDisk(item->edge, item->processor->id);
         } else if (item->type == eventType::OnReadStart) {
             locateToThisProcessorFromDisk(item->edge, item->processor->id);
         }
     } */
  //  cout << "best ended, cluster state now:" << endl;
 //   cluster->printProcessorsEvents();
  //  cout << "Event queus state:" << endl;
  //  events.printAll();
    for (int j = 0; j < vertex->in_degree; j++) {
       assert(//vertex->in_edges[j]->tail->makespan==-1 ||
       bestFinishTime> vertex->in_edges[j]->tail->makespan);
     }


    return bestEvents;
}

void checkBestEvents(vector<shared_ptr<Event>> &bestEvents) {
    for (auto &item: bestEvents) {
        cout << item->id << " at "<< item->getExpectedTimeFire()<<", "<<endl;
        if(item->id.find("-w-f") != std::string::npos){
            auto itWriteStart = std::find_if(bestEvents.begin(), bestEvents.end(),
                                             [item](shared_ptr<Event> e) { return e->id==item->id.substr(0,item->id.length()-4)+"-w-s"; });
            if(itWriteStart != bestEvents.end()){
                auto actualLength = item->getActualTimeFire() - (*itWriteStart)->getActualTimeFire();
                assert(abs(actualLength- item->edge->weight/ item->processor->writeSpeedDisk) < 0.00001);
            }else {
                throw runtime_error("no pair found ofr "+item->id);
            }

        }
        if(item->id.find("-r-f") != std::string::npos){
            auto itReadStart = std::find_if(bestEvents.begin(), bestEvents.end(),
                                             [item](shared_ptr<Event> e) { return e->id==item->id.substr(0,item->id.length()-4)+"-r-s"; });
            if(itReadStart != bestEvents.end()){
                auto actualLength = item->getActualTimeFire() - (*itReadStart)->getActualTimeFire();
                assert(abs(actualLength- item->edge->weight/ item->processor->readSpeedDisk) < 0.00001);
             cout<<"ys"<<endl;
            }else {
                throw runtime_error("no pair found ofr "+item->id);
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
    if(vertex->name=="SORT_BAM_00000011" && ourModifiedProc->id==11){
        cout<<endl;
    }

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
    double amountToOffloadWithoutBiggestFile=0;
    double amountToOffloadWithoutAllFiles =0;
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
        if( getSumIn(vertex)> ourModifiedProc->getAvailableMemory()){
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
                cout << "No pending memories that are not incoming edges of task vertex, no sense in evicting anyone"
                     << endl;

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
            // actuallyUsedMemory = ourModifiedProc->getAvailableMemory();
            //   ourModifiedProc->setAvailableMemory(0);
            resultingVar=1;
        } else if (timeToFinishBiggestEvicted == minTTF) {
            std::pair<shared_ptr<Event>, shared_ptr<Event>> writeEvents;
            auto it = scheduleWriteForEdge(ourModifiedProc, biggestPendingEdge, writeEvents);
            newEvents.emplace_back(writeEvents.first);
            newEvents.emplace_back(writeEvents.second);
            resultingVar = 2;
            assert(startTime <= startTimeFor1Evicted);
            startTime = startTimeFor1Evicted;
            //  actuallyUsedMemory = min (ourModifiedProc->getAvailableMemory()+ biggestFileWeight,  peakMemoryRequirementOfVertex(vertex) );
            // ourModifiedProc->setAvailableMemory(max(0.0,
            //                                          ourModifiedProc->getAvailableMemory()-
            //                                        peakMemoryRequirementOfVertex(vertex)
            //                                         )
            //                                  );
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
                    //our incoming edge
                    //ourModifiedProc->setAvailableMemory(edge);
                    temp.insert(edge);
                    ourModifiedProc->removePendingMemory(edge);
                    //ourModifiedProc->getPendingMemories().erase(ourModifiedProc->getPendingMemories().begin());
                } else {
                    std::pair<shared_ptr<Event>, shared_ptr<Event>> writeEvents;
                    scheduleWriteForEdge(ourModifiedProc, edge, writeEvents);
                    newEvents.emplace_back(writeEvents.first);
                    newEvents.emplace_back(writeEvents.second);
                }
            }
            //ourModifiedProc->setPendingMemories(temp);
            for (auto &temEd: temp) {
                ourModifiedProc->addPendingMemory(temEd);
            }


            //assert(abs(ourModifiedProc->getReadyTimeWrite() - (initTimeWRite + timeToWriteAllPending))<0.001);
            assert(startTime <= startTimeForAllEvicted);
            startTime = startTimeForAllEvicted;

        }

        processIncomingEdges(vertex, eventStartTask, ourModifiedProc, modifiedProcs, newEvents, startTime);

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
        processIncomingEdges(vertex, eventStartTask, ourModifiedProc, modifiedProcs, newEvents, startTime);

        actuallyUsedMemory = peakMemoryRequirementOfVertex(vertex);
        assert(actuallyUsedMemory <= ourModifiedProc->getMemorySize());
      //  ourModifiedProc->setAvailableMemory(
       //         ourModifiedProc->getAvailableMemory() - peakMemoryRequirementOfVertex(vertex));
    }

    if( eventStartTask->getActualTimeFire()>= eventFinishTask->getActualTimeFire()){
      //  throw runtime_error("event Start taks over EventFinish Task");
    }
    startTime = max(max(ourModifiedProc->getReadyTimeCompute(), ourModifiedProc->getReadyTimeRead()), startTime);

    eventStartTask->setActualTimeFire(startTime);
    eventStartTask->setExpectedTimeFire(startTime);

    eventStartTask->setBothTimesFire(startTime);

    finishTime = startTime + vertex->time / ourModifiedProc->getProcessorSpeed();
    eventFinishTask->setBothTimesFire(finishTime);
    /*   for (int i = 0; i < vertex->in_degree; i++) {
           auto inEdge = vertex->in_edges[i];
           auto it = ourModifiedProc->getPendingMemories().find(inEdge);
           if(it!=ourModifiedProc->getPendingMemories().end()){
               int size = ourModifiedProc->getPendingMemories().size();
               ourModifiedProc->getPendingMemories().erase(it);
               assert(ourModifiedProc->getPendingMemories().size()== size-1);
           }
       } */
    assert(vertex->time==0 || eventStartTask->getActualTimeFire()< eventFinishTask->getActualTimeFire());
    eventStartTask->successors.emplace_back(eventFinishTask);
    for (auto &newEvent: newEvents) {
        if (newEvent->id.find("-f") != std::string::npos) {
          //  cout<<"using event "<<newEvent->id<< " at "<<newEvent->getActualTimeFire() <<" as pred to our start event"<<endl;
            eventStartTask->addPredecessor(newEvent);
        }
    }

    finishTime = eventStartTask->getActualTimeFire() + vertex->time / ourModifiedProc->getProcessorSpeed();
    if(resultingVar==1){
        assert(Res<0);
        finishTime+= abs(Res)/ ourModifiedProc->memoryOffloadingPenalty;
    }
    else if(resultingVar==2){
        finishTime+= amountToOffloadWithoutBiggestFile/ ourModifiedProc->memoryOffloadingPenalty;
    }
    else if(resultingVar==3){
        finishTime+=amountToOffloadWithoutAllFiles/ourModifiedProc->memoryOffloadingPenalty;
    }
    if (vertex->time == 0) {
        finishTime = finishTime + 0.0001;
    }

    eventFinishTask->setBothTimesFire(finishTime);

    newEvents.emplace_back(eventStartTask);
    newEvents.emplace_back(eventFinishTask);

    ourModifiedProc->addEvent(eventStartTask);
    ourModifiedProc->addEvent(eventFinishTask);


    assert(vertex->time==0 || eventStartTask->getActualTimeFire()< eventFinishTask->getActualTimeFire());

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
                     vector<shared_ptr<Event>> &createdEvents, double startTimeOfTask) {
    //cout<<"processing, avail mem "<<ourModifiedProc->getAvailableMemory()<<endl;

    double howMuchWasLoaded = ourModifiedProc->getAvailableMemory();
    int ind = v->in_degree;
    // if(ind>0){
    for (int p = 0; p < ind; p++) {
        edge *incomingEdge = v->in_edges[p];
        vertex_t *predecessor = incomingEdge->tail;
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
                scheduleARead(v, ourEvent, createdEvents, startTimeOfTask, ourModifiedProc, incomingEdge);
            } else {
                auto predProc = findPredecessorsProcessor(incomingEdge, modifiedProcs);
                assert(predProc->id == plannedOnThisProc->id);
                scheduleWriteAndRead(v, ourEvent, createdEvents, startTimeOfTask, ourModifiedProc, incomingEdge,
                                     modifiedProcs);
            }

        } else if (isLocatedOnThisProcessor(incomingEdge, ourModifiedProc->id)) {
            //   cout << "edge " << buildEdgeName(incomingEdge) << " already on proc" << endl;
        } else if (isLocatedOnDisk(incomingEdge)) {

            // schedule a read
            scheduleARead(v, ourEvent, createdEvents, startTimeOfTask, ourModifiedProc, incomingEdge);
        } else if (isLocatedOnAnyProcessor(incomingEdge)) {
            shared_ptr<Processor> predecessorsProc = findPredecessorsProcessor(incomingEdge, modifiedProcs);
            if (predecessorsProc->getAfterPendingMemories().find(incomingEdge) ==
                predecessorsProc->getAfterPendingMemories().end()) {
            //    cout << "edge " << buildEdgeName(incomingEdge) << " not found in after pending mems on proc "
             //        << predecessorsProc->id << endl;
               // throw runtime_error("");
               //
                if (v->name == "PICARD_METRICS_00000908" && ourModifiedProc->id == 3) {
                    cout << endl;
                }
                auto plannedWriteFinishOfIncomingEdge = events.find(buildEdgeName(incomingEdge) + "-w-f");
                assert(plannedWriteFinishOfIncomingEdge!= nullptr);
                std::pair<shared_ptr<Event>, shared_ptr<Event>> readEVents;
                if(plannedWriteFinishOfIncomingEdge->getExpectedTimeFire()> startTimeOfTask &&
                        plannedWriteFinishOfIncomingEdge->getExpectedTimeFire()> ourModifiedProc->getReadyTimeRead() ){
                   // throw runtime_error("Smth wrong with reads");
                   cout<<endl;
                    readEVents = scheduleARead(v, ourEvent, createdEvents, startTimeOfTask, ourModifiedProc,
                                               incomingEdge, plannedWriteFinishOfIncomingEdge->getActualTimeFire());
                    readEVents.first->addPredecessor(plannedWriteFinishOfIncomingEdge);
                } else {
                    readEVents = scheduleARead(v, ourEvent, createdEvents, startTimeOfTask, ourModifiedProc,
                                               incomingEdge);
                    readEVents.first->addPredecessor(plannedWriteFinishOfIncomingEdge);
                    //scheduleWriteAndRead(v, ourEvent, createdEvents, startTimeOfTask, ourModifiedProc, incomingEdge, modifiedProcs);
                }
                assert(readEVents.first->getActualTimeFire()< readEVents.second->getActualTimeFire());
            } else {
                //schedule a write
                scheduleWriteAndRead(v, ourEvent, createdEvents, startTimeOfTask, ourModifiedProc, incomingEdge,
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
    if(v->name=="TRIMGALORE_00000905" && incomingEdge->tail->name=="CHECK_DESIGN_00000924"){
        cout<<endl;
    }
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
           abs(ourModifiedProc->getReadyTimeCompute() - ourModifiedProc->getLastComputeEvent().lock()->getActualTimeFire()) < 0.001 );
    if (estimatedStartOfRead < ourModifiedProc->getReadyTimeCompute() &&
        ourModifiedProc->getAvailableMemory() < incomingEdge->weight) {
        estimatedStartOfRead = ourModifiedProc->getReadyTimeCompute();
    }

    if (atThisTime != -1) {
    //    if(atThisTime<estimatedStartOfRead){
     //       cout<<"BAD AT THIS TIME! Taking estimated instead."<<endl;
      //  }
       // else {
            estimatedStartOfRead = atThisTime;
       // }
    }

    vector<shared_ptr<Event>> newEvents = evictFilesUntilThisFits(ourModifiedProc, incomingEdge->weight);
    if (!newEvents.empty()) {
        cout << "evicted" << newEvents.size()<<endl;

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
        eventStartRead->addPredecessor(ourModifiedProc->getLastReadEvent().lock());
    }

    double estimatedTimeOfFinishRead = estimatedStartOfRead + incomingEdge->weight / ourModifiedProc->readSpeedDisk;

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
    assert(eventFinishRead->getActualTimeFire()== eventFinishRead->getExpectedTimeFire());
    assert(eventFinishRead->getActualTimeFire()> eventStartRead->getActualTimeFire());
    auto actualLength = eventFinishRead->getActualTimeFire() - eventStartRead->getActualTimeFire();
    assert(abs(actualLength- incomingEdge->weight/ ourModifiedProc->readSpeedDisk) < 0.00001);
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
        eventStartWrite->addPredecessor(thisProc->getLastWriteEvent().lock());
        thisProc->getLastWriteEvent().lock()->addSuccessor(eventStartWrite);
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
    thisProc->setLastWriteEvent( eventFinishWrite);

    assert(thisProc->getAvailableMemory() >= 0);
    writeEvents.first = eventStartWrite;
    writeEvents.second = eventFinishWrite;

    if(buildEdgeName(edgeToEvict)=="CHECK_DESIGN_00000799-FASTQC_00000792"){
        cout<<endl;
    }
    assert(eventStartWrite->getActualTimeFire()==eventStartWrite->getExpectedTimeFire());
    assert(eventFinishWrite->getActualTimeFire()== eventStartWrite->getActualTimeFire()+edgeToEvict->weight / thisProc->writeSpeedDisk);
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
           abs(ourModifiedProc->getReadyTimeCompute() - ourModifiedProc->getLastComputeEvent().lock()->getActualTimeFire())<0.001);
    if (estimatedStartOfRead < ourModifiedProc->getReadyTimeCompute() &&
        ourModifiedProc->getAvailableMemory() < incomingEdge->weight) {
        estimatedStartOfRead = ourModifiedProc->getReadyTimeCompute();
    }

    double estimatedStartOfWrite = estimatedStartOfRead - incomingEdge->weight / predecessorsProc->writeSpeedDisk;
    estimatedStartOfWrite = max(estimatedStartOfWrite, predecessorsProc->getReadyTimeWrite());

    if (events.find(incomingEdge->tail->name + "-f") != nullptr) {
        estimatedStartOfWrite = max( estimatedStartOfWrite, events.find(incomingEdge->tail->name + "-f")->getActualTimeFire());
    }
    else{
        assert( incomingEdge->tail->makespan!=-1);
        estimatedStartOfWrite = max( estimatedStartOfWrite,  incomingEdge->tail->makespan);
    }

    estimatedStartOfRead = estimatedStartOfWrite + incomingEdge->weight / predecessorsProc->writeSpeedDisk;
    double estimatedTimeOfReadyRead = estimatedStartOfRead + incomingEdge->weight / ourModifiedProc->readSpeedDisk;

    pair<shared_ptr<Event>, shared_ptr<Event>> readEvents = scheduleARead(v, ourEvent, createdEvents,
                                                                          startTimeOfTask, ourModifiedProc,
                                                                          incomingEdge, estimatedStartOfRead);
    assert(readEvents.second->getActualTimeFire() == estimatedTimeOfReadyRead);

    vector<shared_ptr<Event>> predsOfWrite = vector<shared_ptr<Event>>{};
    if (!predecessorsProc->getLastWriteEvent().expired() && !predecessorsProc->getLastWriteEvent().lock()->isDone){
        predsOfWrite.emplace_back(predecessorsProc->getLastWriteEvent());
    }

    vector<shared_ptr<Event>> succsOfWrite = vector<shared_ptr<Event>>{};

    auto eventStartWrite = Event::createEvent(nullptr, incomingEdge, OnWriteStart, predecessorsProc,
                                              estimatedStartOfWrite, estimatedStartOfWrite, predsOfWrite,
                                              succsOfWrite, false,
                                              buildEdgeName(incomingEdge) + "-w-s");
    assert(eventStartWrite->getActualTimeFire()== estimatedStartOfWrite);

    if (events.find(incomingEdge->tail->name + "-f") != nullptr) {
        eventStartWrite->addPredecessor(events.find(incomingEdge->tail->name + "-f"));
    }

    createdEvents.emplace_back(eventStartWrite);

    double estimatedTimeOfFinishWrite = estimatedStartOfWrite + incomingEdge->weight / predecessorsProc->writeSpeedDisk;

    predsOfWrite = vector<shared_ptr<Event>>{};
    succsOfWrite = vector<shared_ptr<Event>>{};
    if (events.find(v->name + "-s")) {
        succsOfWrite.emplace_back(events.find(v->name + "-s"));
    }
    predsOfWrite.emplace_back(eventStartWrite);
    succsOfWrite.emplace_back(readEvents.first);
    assert(estimatedTimeOfFinishWrite<= readEvents.first->getActualTimeFire());

    auto eventFinishWrite = Event::createEvent(nullptr, incomingEdge, OnWriteFinish, predecessorsProc,
                                               estimatedTimeOfFinishWrite, estimatedTimeOfFinishWrite, predsOfWrite,
                                               succsOfWrite, false,
                                               buildEdgeName(incomingEdge) + "-w-f");
    assert(incomingEdge->weight==0 || estimatedTimeOfFinishWrite> eventStartWrite->getActualTimeFire());

    createdEvents.emplace_back(eventFinishWrite);
    predecessorsProc->setLastWriteEvent(eventFinishWrite);

    predecessorsProc->removePendingMemoryAfter(incomingEdge);
    readEvents.first->addPredecessor(eventFinishWrite);

   // cout << buildEdgeName(incomingEdge)<< " start write at " << eventStartWrite->getActualTimeFire() << " finish at "
   //      << eventFinishWrite->getActualTimeFire() << endl;
    assert(eventStartWrite->getActualTimeFire()< eventFinishWrite->getActualTimeFire());
    assert(eventFinishWrite->getActualTimeFire()== eventStartWrite->getActualTimeFire()+incomingEdge->weight / predecessorsProc->writeSpeedDisk);

}

shared_ptr<Processor> findPredecessorsProcessor(edge_t *incomingEdge, vector<shared_ptr<Processor>> &modifiedProcs) {
    vertex_t *predecessor = incomingEdge->tail;
    auto predecessorsProcessorsId = predecessor->assignedProcessorId;
    //assert(isLocatedOnThisProcessor(incomingEdge, predecessorsProcessorsId));
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
        //checkIfPendingMemoryCorrect(addedProc);
    } else {
        addedProc = *it;
    }
    return addedProc;
}


void Event::fireReadStart() {
   // cout << "firing read start for ";
   // print_edge(this->edge);

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        assert(events.find(this->predecessors.at(0)->id)!=nullptr);
        this->setActualTimeFire(this->getActualTimeFire()+ + std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        assert(this->actualTimeFire>this->predecessors.at(0)->actualTimeFire );
        events.insert(shared_from_this());
    } else {
        removeOurselfFromSuccessors(this);
        double durationOfRead = this->edge->weight / this->processor->readSpeedDisk;
        double expectedTimeFireFinish = this->actualTimeFire + deviation(durationOfRead);
        shared_ptr<Event> finishRead = events.find(buildEdgeName(this->edge) + "-r-f");
        if (finishRead == nullptr) {
            throw runtime_error("NO read finish found for " + this->id);
        } else {
            events.update(buildEdgeName(this->edge) + "-w-f", expectedTimeFireFinish);
        }


        this->isDone = true;
    }


}

void Event::fireReadFinish() {
  //  cout << "firing read finish for ";
  //  print_edge(this->edge);

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        this->setActualTimeFire(this->getActualTimeFire()+ + std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        events.insert(shared_from_this());
    } else {
        removeOurselfFromSuccessors(this);
        locateToThisProcessorFromDisk(this->edge, this->processor->id);
        this->isDone = true;
    }
}

void Event::fireWriteStart() {
   // cout << "firing write start for ";
  //  print_edge(this->edge);
    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        this->setActualTimeFire(this->getActualTimeFire()+ + std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        events.insert(shared_from_this());
    } else {
        removeOurselfFromSuccessors(this);

        double durationOfWrite = this->edge->weight / this->processor->writeSpeedDisk;
        double actualTimeFireFinish = this->actualTimeFire + deviation(durationOfWrite);
        //vector<shared_ptr<Event>> predsOfWrite = std::vector<shared_ptr<Event>>{shared_from_this()};
        //vector<shared_ptr<Event>> succsOfWrite = std::vector<shared_ptr<Event>>{};
        shared_ptr<Event> finishWrite =
                events.find(buildEdgeName(this->edge) + "-w-f");
        if (finishWrite == nullptr) {
            throw runtime_error("NO write finish found for " + this->id);
        } else {

            events.update(buildEdgeName(this->edge) + "-w-f", actualTimeFireFinish);
        }
        this->isDone = true;
    }
}

void Event::fireWriteFinish() {
   // cout << "firing write finish for ";
   // print_edge(this->edge);
    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        this->setActualTimeFire(this->getActualTimeFire()+ + std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        events.insert(shared_from_this());
        //cout << "reiniserting. events now " << endl;
       // events.printAll();
    } else {
        removeOurselfFromSuccessors(this);
        delocateFromThisProcessorToDisk(this->edge, this->processor->id);
        this->isDone = true;
    }
}

void Event::removeOurselfFromSuccessors(Event *us) {
   //   cout << "removing from successors us, " << us->id;

    for (auto successor = successors.begin(); successor != successors.end();) {
        //   cout << " from " << (*successor)->id << "'s predecessors" << endl;
//
        for (auto succspred = (*successor)->predecessors.begin(); succspred != (*successor)->predecessors.end();) {
            if ((*succspred)->id == us->id) {
              //    cout << "removed" << endl;
                succspred = (*successor)->predecessors.erase(succspred);
            } else {
                // Move to the next element
                ++succspred;
            }

        }
        //if ((*successor)->predecessors.empty()) {
        //     cout << "no preds on " << (*successor)->id << ", changing start time to "<<endl;
        //     (*successor)->actualTimeFire = min((*successor)->actualTimeFire, us->actualTimeFire);
        //     cout << (*successor)->actualTimeFire << endl;
        //  }
        successor++;
    }
}


double deviation(double in) {
    return in; //in* 2;
}

void Cluster::printProcessorsEvents() {

    for (const auto &[key, value]: this->processors) {
        if (!value->getEvents().empty()) {
            cout << "Processor " << value->id << "with memory " << value->getMemorySize() << ", speed "
                 << value->getProcessorSpeed() << endl;
            cout << "Events: " << endl;
            if (value->getLastComputeEvent().lock())
                cout << "\t" << value->getLastComputeEvent().lock()->id << " ";
            if (value->getLastReadEvent().lock())
                cout << value->getLastReadEvent().lock()->id << " ";
            if (value->getLastWriteEvent().lock())
                cout << value->getLastWriteEvent().lock()->id;
            cout << endl;
            for (auto &item: value->getEvents()) {
                auto eventPrt = item.second.lock();
                if (eventPrt) {
                    cout << "\t" << eventPrt->id << " " << eventPrt->getExpectedTimeFire() << " "
                         << eventPrt->getActualTimeFire()
                         << endl;
                }
            }

            cout << "\t"
                 << " ready time compute " << value->getReadyTimeCompute()
                 << " ready time read " << value->getReadyTimeRead()
                 << " ready time write " << value->getReadyTimeWrite()
                 //<< " ready time write soft " << value->softReadyTimeWrite
                 //<< " avail memory " << value->availableMemory
                 << " pending in memory " << value->getPendingMemories().size() << " pcs: ";

            for (const auto &item: value->getPendingMemories()) {
                print_edge(item);
            }
            cout << endl;
            cout << "after pending in memory " << value->getAfterPendingMemories().size() << " pcs: ";
            for (const auto &item: value->getAfterPendingMemories()) {
                print_edge(item);
            }
            cout << endl;
        }

    }
}


void Processor::addEvent(std::shared_ptr<Event> event) {
    auto foundIterator = this->eventsOnProc.find(event.get()->id);
    if (foundIterator != eventsOnProc.end() && !foundIterator->second.expired()) {
       //  cout << "event already exists on Processor " << this->id << endl;
        if (event->getActualTimeFire() != foundIterator->second.lock()->getActualTimeFire()) {
        //         cout << "Updating time fire " << endl;
            foundIterator->second.lock()->setBothTimesFire(event->getActualTimeFire());
        }
    } else {
        this->eventsOnProc[event->id] = event;
    }
}

bool dealWithPredecessors(shared_ptr<Event> us) {
    if (!us->predecessors.empty()) {

        auto it = us->predecessors.begin();
        while (it != us->predecessors.end()) {
            if ((*it)->isDone) {
                it = us->predecessors.erase(it);
            } else {
                it++;
            }
        }
       // cout << "predecessors not empty for " << us->id << endl;
        for (const auto &item: us->predecessors) {
      //      cout << "predecessor " << item->id << ", ";
            if (item->getActualTimeFire() > us->getActualTimeFire()) {
                assert(abs(item->getActualTimeFire()-item->getExpectedTimeFire())<1);
                us->setActualTimeFire(item->getActualTimeFire());
            }
        }
     //   cout << endl;

        for (const auto &item: us->successors) {
            item->setActualTimeFire(max(item->getActualTimeFire(), us->getActualTimeFire()));
        }
    }
    return us->predecessors.empty();
}

void transferAfterMemoriesToBefore(shared_ptr<Processor> &ourModifiedProc) {
    ourModifiedProc->resetPendingMemories();
    ourModifiedProc->setAvailableMemory(ourModifiedProc->getMemorySize());
    for (auto &item: ourModifiedProc->getAfterPendingMemories()) {
        ourModifiedProc->addPendingMemory(item);
    }
    ourModifiedProc->setAvailableMemory(ourModifiedProc->getAfterAvailableMemory());
    ourModifiedProc->resetAfterPendingMemories();
    ourModifiedProc->setAfterAvailableMemory(ourModifiedProc->getMemorySize());
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


void Processor::setLastWriteEvent(shared_ptr<Event> lwe){
    this->lastWriteEvent= lwe;
    this->readyTimeWrite= lwe->getActualTimeFire();
}

void Processor::setLastReadEvent(shared_ptr<Event> lre) {
    this->lastReadEvent= lre;
    this->readyTimeRead= lre->getActualTimeFire();
}

void Processor::setLastComputeEvent(shared_ptr<Event> lce) {
    this->lastComputeEvent= lce;
    this->readyTimeCompute= lce->getActualTimeFire();
}