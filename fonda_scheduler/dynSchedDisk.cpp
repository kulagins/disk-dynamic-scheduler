
#include <queue>
#include "fonda_scheduler/dynSchedDisk.hpp"
#include "fonda_scheduler/dynSched.hpp"

Cluster *cluster;
EventManager events;

double new_heuristic_dynamic(graph_t *graph, Cluster *cluster1, int algoNum, bool isHeft) {
    cluster = cluster1;
    algoNum = isHeft ? 1 : algoNum;
    vector<pair<vertex_t *, double>> ranks = calculateBottomLevels(graph, algoNum);
    removeSourceAndTarget(graph, ranks);
    sort(ranks.begin(), ranks.end(),
         [](pair<vertex_t *, double> a, pair<vertex_t *, double> b) {
             return a.second > b.second;

         });
    enforce_single_source_and_target(graph);
    vertex_t *vertex = graph->first_vertex;
    while (vertex != nullptr) {
        if (vertex->in_degree == 0) {
            cout << "starting task " << vertex->name << endl;
            vector<shared_ptr<Processor>> bestModifiedProcs;
            shared_ptr<Processor> bestProcessorToAssign;
            vector<shared_ptr<Event>> newEvents =
                    bestTentativeAssignment(vertex, bestModifiedProcs, bestProcessorToAssign);

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
        cout << "NEXT "; //events.printAll();
        cntr++;
        shared_ptr<Event> firstEvent = events.getEarliest();
        cout << "event " << firstEvent->id << " at " << firstEvent->actualTimeFire << endl;
        bool removed = events.remove(firstEvent->id);
        assert(removed == true);

        firstEvent->fire();

        //cout<<"events now "; events.printAll();
    }
    //  cout<<cntr<<" "<<graph->number_of_vertices<<endl;
    cout << cluster->getMemBiggestFreeProcessor()->readyTimeCompute << endl;

    return -1;
}

void Event::fireTaskStart() {
    string thisid = this->id;
    cout << "firing task start for " << thisid << endl;
    double timeStart = 0;

    if (!this->predecessors.empty()) {
        cout << "predecessors not empty for " << this->id << endl;
        for (const auto &item: predecessors) {
            if (item->actualTimeFire > timeStart) {
                timeStart = item->actualTimeFire;
            }
        }
        this->actualTimeFire = timeStart;
        events.insert(shared_from_this());

        for (const auto &item: successors) {
            item->actualTimeFire = max(item->actualTimeFire, timeStart);
        }
    } else {
        removeOurselfFromSuccessors(this);
        this->task->status = Status::Running;
        auto ourFinishEvent = events.find(this->task->name + "-f");
        if (ourFinishEvent == nullptr) {
            throw runtime_error("NOt found finish event to " + this->task->name);
        }

        double actualRuntime = deviation(this->task->time / this->processor->getProcessorSpeed());
        double d = this->actualTimeFire + actualRuntime;
        ourFinishEvent->actualTimeFire = d;
        // this->processor->addEvent(taskWrite);
        // this->processor->lastComputeEvent = taskWrite;
        // this->processor->setReadyTimeCompute(d);
    }
}


void Event::fireTaskFinish() {
    vertex_t *thisTask = this->task;
    cout << "firing task Finish for " << this->id << endl;

    // free its memory TODO ??
  //  this->processor->availableMemory += task->actuallyUsedMemory;
    assert(this->processor->availableMemory<= this->processor->getMemorySize());
    //set its status to finished
    thisTask->status = Status::Finished;
    string thisId = this->id;
    removeOurselfFromSuccessors(this);
    for (const shared_ptr<Event> &successor: this->successors) {
        // updates successors' fire time
        successor->actualTimeFire = this->actualTimeFire;
    }
    cout << "EVENTS " << endl;
    for (const auto &item: this->processor->getEvents()) {
        cout << item.first << "\t";
    }
    cout << endl;

    for (int i = 0; i < thisTask->out_degree; i++) {
        this->processor->loadFromNowhere(thisTask->out_edges[i]);
        assert(this->processor->pendingMemories.find(thisTask->out_edges[i])
               != this->processor->pendingMemories.end());
        assert(cluster->getProcessorById(this->processor->id)->pendingMemories.find(thisTask->out_edges[i])
               != cluster->getProcessorById(this->processor->id)->pendingMemories.end());
    }

    //Then task goes over its successor tasks in the workflow and schedules ready ones.
    for (int i = 0; i < thisTask->out_degree; i++) {
        vertex_t *childTask = thisTask->out_edges[i]->head;
        cout << "deal with child " << childTask->name << endl;
        bool isReady = true;
        for (int j = 0; j < thisTask->in_degree; j++) {
            if (childTask->in_edges[j]->tail->status == Status::Unscheduled) {
                isReady = false;
            }
        }

        std::vector<shared_ptr<Event> > pred, succ;
        //TODO Can be two times in queue?
        if (isReady) {
            vector<shared_ptr<Processor>> bestModifiedProcs;
            shared_ptr<Processor> bestProcessorToAssign;
            vector<shared_ptr<Event>> newEvents =
                    bestTentativeAssignment(childTask, bestModifiedProcs, bestProcessorToAssign);

            for (const auto &item: newEvents) {
                events.insert(item);
                item->processor->addEvent(item);
            }
            childTask->status = Status::Scheduled;
        }
    }
}


vector<shared_ptr<Event>> bestTentativeAssignment(vertex_t *vertex, vector<shared_ptr<Processor>> &bestModifiedProcs,
                                                  shared_ptr<Processor> &bestProcessorToAssign) {
    cout << "best tent assign for " << vertex->name << endl;
    double bestStartTime = numeric_limits<double>::max(), bestFinishTime = numeric_limits<double>::max(),
    bestReallyUsedMem;
    vector<shared_ptr<Event>> bestEvents;
    int resultingVar;

    for (auto &[id, processor]: cluster->getProcessors()) {
        double finTime = -1, startTime = -1, reallyUsedMem=0;
        double ftBefore = processor->readyTimeCompute;
        int resultingEvictionVariant = -1;
        auto ourModifiedProc = make_shared<Processor>(*processor);
        vector<shared_ptr<Event>> newEvents = {};
        //checkIfPendingMemoryCorrect(ourModifiedProc);
        vector<shared_ptr<Processor>> modifiedProcs = tentativeAssignment(vertex, ourModifiedProc,
                                                                          finTime,
                                                                          startTime, resultingEvictionVariant,
                                                                          newEvents, reallyUsedMem);

        if (bestFinishTime > finTime) {
            cout << "best acutalize to " << ourModifiedProc->id << endl;
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

    for (auto &item: bestEvents) {
        events.insert(item);
    }

    for (auto &item: bestModifiedProcs) {
        auto iterator = cluster->getProcessors().find(item->id);
        iterator->second->updateFrom(*item);
        assert(iterator->second->pendingMemories.size() == item->pendingMemories.size());
    }

    cout << " new events " << endl;

    for (auto &item: bestEvents) {
        cout << item->id << ", ";
    }
    cout << endl;

    for (auto &item: bestEvents) {
        //assert(item->processor->pendingMemories.size())
        if (item->type == eventType::OnWriteStart) {
            delocateFromThisProcessorToDisk(item->edge, item->processor->id);
        } else if (item->type == eventType::OnReadStart) {
            locateToThisProcessorFromDisk(item->edge, item->processor->id);
        }

    }


    vertex->assignedProcessorId = bestProcessorToAssign->id;
    vertex->actuallyUsedMemory =bestReallyUsedMem;
    cout << "best ended, cluster state now:" << endl;
    cluster->printProcessorsEvents();
    cout << "Event queus state:" << endl;
    events.printAll();
    return bestEvents;
}

vector<shared_ptr<Processor>>
tentativeAssignment(vertex_t *vertex, shared_ptr<Processor> ourModifiedProc,
                    double &finTime, double &startTime, int &resultingVar, vector<shared_ptr<Event>> &newEvents,
                    double & actuallyUsedMemory) {
    cout << "try " << ourModifiedProc->id << " for " << vertex->name << endl;
    if(vertex->name=="A2" && ourModifiedProc->id==2){
        cout<<endl;
    }
    assert(ourModifiedProc->availableMemory<= ourModifiedProc->getMemorySize());

    vector<std::shared_ptr<Processor>> modifiedProcs;
    modifiedProcs.emplace_back(ourModifiedProc);

    startTime = ourModifiedProc->readyTimeCompute;

    vector<shared_ptr<Event>> preds = vector<shared_ptr<Event>>{};
    vector<shared_ptr<Event>> succs = vector<shared_ptr<Event>>{};
    auto eventStartTask = make_shared<Event>(vertex, nullptr, OnTaskStart, ourModifiedProc,
                                             startTime, startTime, preds,
                                             succs, false,
                                             vertex->name + "-s");
    auto finishTime = startTime + vertex->time / ourModifiedProc->getProcessorSpeed();
    auto eventFinishTask = make_shared<Event>(vertex, nullptr, OnTaskFinish, ourModifiedProc,
                                              finishTime, finishTime, preds,
                                              succs, false,
                                              vertex->name + "-f");

    double sumOut = getSumOut(vertex);
    if (ourModifiedProc->getMemorySize() < sumOut) {
        //  cout<<"too large outs absolutely"<<endl;
        finTime = std::numeric_limits<double>::max();
        return {};
    }
    realSurplusOfOutgoingEdges(vertex, ourModifiedProc, sumOut);
    double biggestFileWeight;
    double Res = howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(vertex, ourModifiedProc);
    if (Res < 0) {

        double amountToOffload = -Res;
        double shortestFT = std::numeric_limits<double>::max();

        double timeToFinishNoEvicted = startTime + vertex->time / ourModifiedProc->getProcessorSpeed() +
                                       amountToOffload / ourModifiedProc->memoryOffloadingPenalty;
        assert(timeToFinishNoEvicted > startTime);
        if (sumOut > ourModifiedProc->availableMemory) {
            //cout<<"cant"<<endl;
            timeToFinishNoEvicted = std::numeric_limits<double>::max();
        }


        double timeToFinishBiggestEvicted = std::numeric_limits<double>::max(),
                timeToFinishAllEvicted = std::numeric_limits<double>::max();
        double timeToWriteAllPending = 0;

        double startTimeFor1Evicted, startTimeForAllEvicted;
        startTimeFor1Evicted = startTimeForAllEvicted = ourModifiedProc->readyTimeWrite > startTime ?
                                                        ourModifiedProc->readyTimeWrite : startTime;

        edge_t *biggestPendingEdge = *ourModifiedProc->pendingMemories.begin();
        if (!ourModifiedProc->pendingMemories.empty()) {
            assert(biggestPendingEdge->weight >=
                   (*ourModifiedProc->pendingMemories.rbegin())->weight);
            biggestFileWeight = biggestPendingEdge->weight;
            double amountToOffloadWithoutBiggestFile = (amountToOffload - biggestFileWeight) > 0 ? (amountToOffload -
                                                                                                    biggestFileWeight)
                                                                                                 : 0;
            double finishTimeToWrite = ourModifiedProc->readyTimeWrite +
                                       biggestFileWeight / ourModifiedProc->writeSpeedDisk;
            startTimeFor1Evicted = max(startTime, finishTimeToWrite);
            timeToFinishBiggestEvicted =
                    startTimeFor1Evicted
                    + vertex->time / ourModifiedProc->getProcessorSpeed() +
                    amountToOffloadWithoutBiggestFile / ourModifiedProc->memoryOffloadingPenalty;
            assert(timeToFinishBiggestEvicted > startTimeFor1Evicted);

            double availableMemWithoutBiggest = ourModifiedProc->availableMemory + biggestFileWeight;
            if (sumOut > availableMemWithoutBiggest)
                timeToFinishBiggestEvicted = std::numeric_limits<double>::max();


            double sumWeightsOfAllPending = 0;
            for (const auto &item: ourModifiedProc->pendingMemories) {
                cout<<timeToWriteAllPending<<endl;
                timeToWriteAllPending += item->weight / ourModifiedProc->writeSpeedDisk;

                sumWeightsOfAllPending += item->weight;
            }

            double amountToOffloadWithoutAllFiles = (amountToOffload - sumWeightsOfAllPending > 0) ?
                                                    amountToOffload - sumWeightsOfAllPending : 0;

            assert(amountToOffloadWithoutAllFiles >= 0);
            finishTimeToWrite = ourModifiedProc->readyTimeWrite +
                                timeToWriteAllPending;
            startTimeForAllEvicted = max(startTimeForAllEvicted, finishTimeToWrite);
            timeToFinishAllEvicted = startTimeForAllEvicted + vertex->time / ourModifiedProc->getProcessorSpeed() +
                                     amountToOffloadWithoutAllFiles / ourModifiedProc->memoryOffloadingPenalty;
            assert(timeToFinishAllEvicted > startTimeForAllEvicted);

        }

        double minTTF = min(timeToFinishNoEvicted, min(timeToFinishBiggestEvicted, timeToFinishAllEvicted));
        if (minTTF == std::numeric_limits<double>::max()) {
            cout << "minTTF inf" << endl;
            finTime = std::numeric_limits<double>::max();
            return {};
        }

       // ourModifiedProc->readyTimeCompute = minTTF;
       // finTime = ourModifiedProc->readyTimeCompute;
        //assert(ourModifiedProc->readyTimeCompute < std::numeric_limits<double>::max());



        if (timeToFinishNoEvicted == minTTF) {
            actuallyUsedMemory = ourModifiedProc->availableMemory;
        } else if (timeToFinishBiggestEvicted == minTTF) {
            std::pair<shared_ptr<Event>, shared_ptr<Event>> writeEvents;
            auto it = scheduleWriteForEdge(ourModifiedProc, biggestPendingEdge, writeEvents);
            newEvents.emplace_back(writeEvents.first);
            newEvents.emplace_back(writeEvents.second);
            resultingVar = 2;
            assert(startTime <= startTimeFor1Evicted);
            startTime = startTimeFor1Evicted;
            actuallyUsedMemory = ourModifiedProc->availableMemory+ biggestFileWeight;
            assert(biggestPendingEdge != nullptr);
            assert(!biggestPendingEdge->locations.empty());
            assert(biggestPendingEdge->head->status==Status::Scheduled ||
                    isLocatedOnThisProcessor(biggestPendingEdge, ourModifiedProc->id));
        } else if (timeToFinishAllEvicted == minTTF) {
            cout<<"case 3"<<endl;
            resultingVar = 3;
            double initTimeWRite = ourModifiedProc->readyTimeWrite;
            while(!ourModifiedProc->pendingMemories.empty()){
                edge_t * edge = (*ourModifiedProc->pendingMemories.begin());
                cout << buildEdgeName(edge) << endl;
                std::pair<shared_ptr<Event>, shared_ptr<Event>> writeEvents;
                scheduleWriteForEdge(ourModifiedProc, edge, writeEvents);
                newEvents.emplace_back(writeEvents.first);
                newEvents.emplace_back(writeEvents.second);
                actuallyUsedMemory= min(ourModifiedProc->getMemorySize(), peakMemoryRequirementOfVertex(vertex));
            }
            assert(ourModifiedProc->readyTimeWrite ==  initTimeWRite + timeToWriteAllPending);
            assert(startTime <= startTimeForAllEvicted);
            startTime = startTimeForAllEvicted;

        }
    }

    processIncomingEdges(vertex, eventStartTask, ourModifiedProc, modifiedProcs, newEvents, startTime);


    startTime = max(max(ourModifiedProc->readyTimeCompute, ourModifiedProc->readyTimeRead), startTime);

    eventStartTask->actualTimeFire = eventStartTask->expectedTimeFire = startTime;
    finishTime = startTime + vertex->time / ourModifiedProc->getProcessorSpeed();
    if (vertex->time == 0) {
        finishTime = finishTime + 0.0001;
    }

    eventFinishTask->actualTimeFire = eventFinishTask->expectedTimeFire = finishTime;


    eventFinishTask->predecessors.emplace_back(eventStartTask);
    ourModifiedProc->lastComputeEvent = eventFinishTask;
    ourModifiedProc->readyTimeCompute = finishTime;

    eventStartTask->successors.emplace_back(eventFinishTask);
    for (auto &newEvent: newEvents) {
        if (newEvent->id.find("-f") != std::string::npos) {
            eventStartTask->predecessors.emplace_back(newEvent);
        }
    }
    newEvents.emplace_back(eventStartTask);
    newEvents.emplace_back(eventFinishTask);

    ourModifiedProc->addEvent(eventStartTask);
    ourModifiedProc->addEvent(eventFinishTask);

    finTime = ourModifiedProc->readyTimeCompute;

    return modifiedProcs;
}


double
processIncomingEdges(const vertex_t *v, shared_ptr<Event> ourEvent, shared_ptr<Processor> &ourModifiedProc,
                     vector<std::shared_ptr<Processor>> &modifiedProcs,
                     vector<shared_ptr<Event>> createdEvents, double startTimeOfTask) {
    /*if (!v) {
        cout << "Error: v is null!" << endl;
        return -1;
    }
    cout<<v->name<<endl;
    cout<<v->in_degree<<endl;
    cout<<"in before "<<v->in_degree<<endl;

    printf("v pointer address: %p\n", (void*)v);
    printf("v->in_degree address: %p, value: %d\n", (void*)&v->in_degree, v->in_degree);
    if (!v) {
        cout << "v is null!" << endl;
    } else {
        cout << "v exists." << endl;
    }

    cout << "v address: " << v << endl;
    cout << "&v->in_degree: " << &v->in_degree << " value: " << v->in_degree << endl;

    assert(reinterpret_cast<std::uintptr_t>(v) % alignof(decltype(*v)) == 0);
    assert(reinterpret_cast<std::uintptr_t>(&v->in_degree) % alignof(decltype(v->in_degree)) == 0);

    std::cout << "Before loop: v->in_degree = " << v->in_degree << std::endl;
    std::cout << "v address: " << v << ", v->address: " << &v->in_degree << std::endl;
    std::cout << "v->name: " << v->name << std::endl;
    std::cout << "v->in_degree: " << v->in_degree << std::endl;



    cout<< (ourEvent?"yes our Ev ": "no our Ev ")<< ourModifiedProc->id<<" "<<
           modifiedProcs.size()<<" "<<
           createdEvents.size()<< " "<<startTimeOfTask<<endl; */
    int ind = v->in_degree;
    // if(ind>0){
    for (int p = 0; p < ind; p++) {
        int j = 0;
        cout << "Loop start..." << endl;
        edge *incomingEdge = v->in_edges[j];
        vertex_t *predecessor = incomingEdge->tail;
        if (isLocatedNowhere(incomingEdge)) {
            throw runtime_error("Edge located nowhere");
        } else if (isLocatedOnThisProcessor(incomingEdge, ourModifiedProc->id)) {
            cout << "edge " << buildEdgeName(incomingEdge) << " already on proc" << endl;
        } else if (isLocatedOnDisk(incomingEdge)) {
            // schedule a read
            scheduleARead(v, ourEvent, createdEvents, startTimeOfTask, ourModifiedProc, incomingEdge);
        } else if (isLocatedOnAnyProcessor(incomingEdge)) {
            //schedule a write
            scheduleWriteAndRead(v, ourEvent, createdEvents, startTimeOfTask, ourModifiedProc, incomingEdge,
                                 modifiedProcs);
        }
    }
    //}
    return 0;
}

std::pair<shared_ptr<Event>, shared_ptr<Event>>
scheduleARead(const vertex_t *v, shared_ptr<Event> ourEvent, vector<shared_ptr<Event>> &createdEvents,
              double startTimeOfTask,
              shared_ptr<Processor> &ourModifiedProc, edge *&incomingEdge, double atThisTime) {
    double estimatedStartOfRead = startTimeOfTask - incomingEdge->weight / ourModifiedProc->readSpeedDisk;
    estimatedStartOfRead = max(estimatedStartOfRead, ourModifiedProc->readyTimeRead);
    vector<shared_ptr<Event>> predsOfRead = vector<shared_ptr<Event>>{};
    vector<shared_ptr<Event>> succsOfRead = vector<shared_ptr<Event>>{events.find(v->name + "-s")};

    //if this start of the read is happening during the runtime  of the previous task
//TODO WHAT IF BEFORE IT STARTS?
    assert(ourModifiedProc->lastComputeEvent.expired() ||
           ourModifiedProc->readyTimeCompute == ourModifiedProc->lastComputeEvent.lock()->actualTimeFire);
    if (estimatedStartOfRead < ourModifiedProc->readyTimeCompute &&
        ourModifiedProc->availableMemory < incomingEdge->weight) {
        estimatedStartOfRead = ourModifiedProc->readyTimeCompute;
    }

    if (atThisTime != -1) {
        estimatedStartOfRead = atThisTime;
    }

    vector<shared_ptr<Event>> newEvents = evictFilesUntilThisFits(ourModifiedProc, incomingEdge->weight);
    if (!newEvents.empty()) {
        cout << "evicted" << endl;
    }
    createdEvents.insert(createdEvents.end(), newEvents.begin(), newEvents.end());

    //??????If the starting time of the read is during the execution of the previous task and there
    // is enough memory to read this file, then we move the read forward to the beginning of this previous task.
    auto eventStartRead = make_shared<Event>(nullptr, incomingEdge, OnReadStart, ourModifiedProc,
                                             estimatedStartOfRead, estimatedStartOfRead, predsOfRead,
                                             succsOfRead, false,
                                             buildEdgeName(incomingEdge) + "-r-s");
    createdEvents.emplace_back(eventStartRead);
    if (!ourModifiedProc->lastReadEvent.expired()) {
        eventStartRead->predecessors.emplace_back(ourModifiedProc->lastReadEvent);
    }

    double estimatedTimeOfFinishRead = estimatedStartOfRead + incomingEdge->weight / ourModifiedProc->readSpeedDisk;

    predsOfRead = vector<shared_ptr<Event>>{eventStartRead};
    succsOfRead = vector<shared_ptr<Event>>{events.find(v->name + "-s")};

    auto eventFinishRead = make_shared<Event>(nullptr, incomingEdge, OnReadFinish, ourModifiedProc,
                                              estimatedTimeOfFinishRead, estimatedTimeOfFinishRead, predsOfRead,
                                              succsOfRead, false,
                                              buildEdgeName(incomingEdge) + "-r-f");
    createdEvents.emplace_back(eventFinishRead);
    eventFinishRead->successors.emplace_back(ourEvent);
    ourModifiedProc->readyTimeRead = estimatedTimeOfFinishRead;
    ourModifiedProc->lastReadEvent = eventFinishRead;
    ourModifiedProc->addPendingMemory(incomingEdge);
    return {eventStartRead, eventFinishRead};
}


vector<shared_ptr<Event>> evictFilesUntilThisFits(shared_ptr<Processor> thisProc, double weightToFit) {
    vector<shared_ptr<Event>> newEvents;
    while (thisProc->availableMemory < weightToFit &&!thisProc->pendingMemories.empty() ) {
        edge_t *edgeToEvict = *thisProc->pendingMemories.begin();
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
    vector<shared_ptr<Event>> pred, succ = vector<shared_ptr<Event>>{};
    auto eventStartWrite = make_shared<Event>(nullptr, edgeToEvict, OnWriteStart, thisProc,
                                              thisProc->readyTimeWrite, thisProc->readyTimeWrite,
                                              pred,
                                              succ, false,
                                              buildEdgeName(edgeToEvict) + "-w-s");


    if (!thisProc->lastWriteEvent.expired()) {
        eventStartWrite->predecessors.emplace_back(thisProc->lastWriteEvent);
        thisProc->lastWriteEvent.lock()->successors.emplace_back(eventStartWrite);
    }


    double timeFinishWrite = thisProc->readyTimeWrite + edgeToEvict->weight / thisProc->writeSpeedDisk;
    auto eventFinishWrite = make_shared<Event>(nullptr, edgeToEvict, OnWriteStart, thisProc,
                                               timeFinishWrite,
                                               timeFinishWrite,
                                               pred,
                                               succ, false,
                                               buildEdgeName(edgeToEvict) + "-w-f");

    eventFinishWrite->predecessors.emplace_back(eventStartWrite);
    thisProc->readyTimeWrite = timeFinishWrite;
    thisProc->lastWriteEvent = eventFinishWrite;
    thisProc->availableMemory -= edgeToEvict->weight;
    assert(thisProc->availableMemory>=0);
    writeEvents.first = eventStartWrite;
    writeEvents.second = eventFinishWrite;
    return thisProc->removePendingMemory(edgeToEvict);
}

void scheduleWriteAndRead(const vertex_t *v, shared_ptr<Event> ourEvent, vector<shared_ptr<Event>> &createdEvents,
                          double startTimeOfTask,
                          shared_ptr<Processor> &ourModifiedProc, edge *&incomingEdge,
                          vector<std::shared_ptr<Processor>> &modifiedProcs) {

    shared_ptr<Processor> predecessorsProc = findPredecessorsProcessor(incomingEdge, modifiedProcs);
    double estimatedStartOfRead = startTimeOfTask - incomingEdge->weight / ourModifiedProc->readSpeedDisk;
    estimatedStartOfRead = max(estimatedStartOfRead, ourModifiedProc->readyTimeRead);

    assert(ourModifiedProc->lastComputeEvent.expired() ||
           ourModifiedProc->readyTimeCompute == ourModifiedProc->lastComputeEvent.lock()->actualTimeFire);
    if (estimatedStartOfRead < ourModifiedProc->readyTimeCompute &&
        ourModifiedProc->availableMemory < incomingEdge->weight) {
        estimatedStartOfRead = ourModifiedProc->readyTimeCompute;
    }

    double estimatedStartOfWrite = estimatedStartOfRead - incomingEdge->weight / predecessorsProc->writeSpeedDisk;
    estimatedStartOfWrite = max(estimatedStartOfWrite, predecessorsProc->readyTimeWrite);

    estimatedStartOfRead = estimatedStartOfWrite + incomingEdge->weight / predecessorsProc->writeSpeedDisk;
    double estimatedTimeOfReadyRead = estimatedStartOfRead + incomingEdge->weight / ourModifiedProc->readSpeedDisk;

    pair<shared_ptr<Event>, shared_ptr<Event>> readEvents = scheduleARead(v, ourEvent, createdEvents,
                                                                          startTimeOfTask, ourModifiedProc,
                                                                          incomingEdge, estimatedStartOfRead);
    assert(readEvents.second->actualTimeFire == estimatedTimeOfReadyRead);

    vector<shared_ptr<Event>> predsOfWrite = vector<shared_ptr<Event>>{};
    if (!predecessorsProc->lastWriteEvent.expired())
        predsOfWrite.emplace_back(predecessorsProc->lastWriteEvent);
    vector<shared_ptr<Event>> succsOfWrite = vector<shared_ptr<Event>>{};

    auto eventStartWrite = make_shared<Event>(nullptr, incomingEdge, OnWriteStart, predecessorsProc,
                                              estimatedStartOfWrite, estimatedStartOfWrite, predsOfWrite,
                                              succsOfWrite, false,
                                              buildEdgeName(incomingEdge) + "-w-s");
    createdEvents.emplace_back(eventStartWrite);

    double estimatedTimeOfFinishWrite = estimatedStartOfWrite + incomingEdge->weight / predecessorsProc->writeSpeedDisk;

    predsOfWrite = vector<shared_ptr<Event>>{};
    succsOfWrite = vector<shared_ptr<Event>>{events.find(v->name + "-s")};
    predsOfWrite.emplace_back(eventStartWrite);
    succsOfWrite.emplace_back(readEvents.first);

    auto eventFinishWrite = make_shared<Event>(nullptr, incomingEdge, OnReadFinish, ourModifiedProc,
                                               estimatedTimeOfFinishWrite, estimatedTimeOfFinishWrite, predsOfWrite,
                                               succsOfWrite, false,
                                               buildEdgeName(incomingEdge) + "-w-f");
    createdEvents.emplace_back(eventFinishWrite);
    predecessorsProc->readyTimeWrite = estimatedTimeOfFinishWrite;
    predecessorsProc->lastWriteEvent = eventFinishWrite;
    predecessorsProc->removePendingMemory(incomingEdge);

    readEvents.first->predecessors.emplace_back(eventFinishWrite);

}

shared_ptr<Processor> findPredecessorsProcessor(edge_t *incomingEdge, vector<shared_ptr<Processor>> &modifiedProcs) {
    vertex_t *predecessor = incomingEdge->tail;
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
        //checkIfPendingMemoryCorrect(addedProc);
    } else {
        addedProc = *it;
    }
    return addedProc;
}


void Event::fireReadStart() {
    cout << "firing read start for ";
    print_edge(this->edge);
    removeOurselfFromSuccessors(this);

    double durationOfRead = this->edge->weight / this->processor->readSpeedDisk;
    double expectedTimeFireFinish = this->actualTimeFire + deviation(durationOfRead);
    vector<shared_ptr<Event>> predsOfFinish = std::vector<shared_ptr<Event>>{shared_from_this()};
    vector<shared_ptr<Event>> succsOfFinish = std::vector<shared_ptr<Event>>{};
    shared_ptr<Event> finishRead = make_shared<Event>(this->edge->head, nullptr, eventType::OnWriteFinish,
                                                      this->processor, expectedTimeFireFinish,
                                                      expectedTimeFireFinish,
                                                      predsOfFinish,
                                                      succsOfFinish, false, buildEdgeName(this->edge) + "-e-f");
    events.insert(finishRead);

}

void Event::fireReadFinish() {
    cout << "firing read finish for ";
    print_edge(this->edge);
    removeOurselfFromSuccessors(this);

}

void Event::fireWriteStart() {
    cout << "firing write start for ";
    print_edge(this->edge);
    removeOurselfFromSuccessors(this);

    double durationOfWrite = this->edge->weight / this->processor->writeSpeedDisk;
    double expectedTimeFireFinish = this->actualTimeFire + deviation(durationOfWrite);
    vector<shared_ptr<Event>> predsOfWrite = std::vector<shared_ptr<Event>>{shared_from_this()};
    vector<shared_ptr<Event>> succsOfWrite = std::vector<shared_ptr<Event>>{};
    shared_ptr<Event> finishWrite = make_shared<Event>(this->edge->head, nullptr, eventType::OnWriteFinish,
                                                       this->processor, expectedTimeFireFinish,
                                                       expectedTimeFireFinish,
                                                       predsOfWrite,
                                                       succsOfWrite, false, buildEdgeName(this->edge) + "-w-f");

    events.insert(finishWrite);
}

void Event::fireWriteFinish() {
    cout << "firing write finish for ";
    print_edge(this->edge);
    removeOurselfFromSuccessors(this);
}

void Event::removeOurselfFromSuccessors(Event *us) {
    cout << "removing " << us->id;

    for (auto successor = successors.begin(); successor != successors.end();) {
        cout << " from " << (*successor)->id << "'s predecessors" << endl;

        for (auto succspred = (*successor)->predecessors.begin(); succspred != (*successor)->predecessors.end();) {
            if ((*succspred)->id == us->id) {
                cout << "removed" << endl;
                succspred = (*successor)->predecessors.erase(succspred);
            } else {
                // Move to the next element
                ++succspred;
            }

        }
        if ((*successor)->predecessors.empty()) {
            cout << "no preds on " << (*successor)->id << ", changing start time to ";
            (*successor)->actualTimeFire = min((*successor)->actualTimeFire, us->actualTimeFire);
            cout << (*successor)->actualTimeFire << endl;
        }
        successor++;
    }
}


double deviation(double in) {
    return in; //in* 2;
}

void Cluster::printProcessorsEvents() {
    {

        for (const auto &[key, value]: this->processors) {
            if (!value->getEvents().empty()) {
                cout << "Processor " << value->id << "with memory " << value->getMemorySize() << ", speed "
                     << value->getProcessorSpeed() << endl;
                cout << "Events: " << endl;
                if (value->lastComputeEvent.lock())
                    cout << "\t" << value->lastComputeEvent.lock()->id << " ";
                if (value->lastReadEvent.lock())
                    cout << value->lastReadEvent.lock()->id << " ";
                if (value->lastWriteEvent.lock())
                    cout << value->lastWriteEvent.lock()->id;
                cout << endl;
                for (auto &item: value->getEvents()) {
                    auto eventPrt = item.second.lock();
                    if (eventPrt) {
                        cout << "\t" << eventPrt->id << " " << eventPrt->expectedTimeFire << " "
                             << eventPrt->actualTimeFire
                             << endl;
                    }
                }

                cout << "\t"
                     << " ready time compute " << value->readyTimeCompute
                     << " ready time read " << value->readyTimeRead
                     << " ready time write " << value->readyTimeWrite
                     //<< " ready time write soft " << value->softReadyTimeWrite
                     //<< " avail memory " << value->availableMemory
                     << " pending in memory " << value->pendingMemories.size() << " pcs: ";

                for (const auto &item: value->pendingMemories) {
                    print_edge(item);
                }
                cout << endl;
            }

        }
    }
}


void Processor::addEvent(std::shared_ptr<Event> event) {
    auto foundIterator = this->events.find(event.get()->id);
    if (foundIterator != events.end()) {
        cout << "event already exists on Processor " << this->id << endl;
        if (event->actualTimeFire != foundIterator->second.lock()->actualTimeFire) {
            cout << "Updating time fire " << endl;
            foundIterator->second.lock()->actualTimeFire = event->actualTimeFire;
        }
    } else {
        this->events[event->id] = event;
    }
}