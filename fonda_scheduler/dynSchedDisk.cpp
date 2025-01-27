
#include <queue>
#include "fonda_scheduler/dynSchedDisk.hpp"
#include "fonda_scheduler/dynSched.hpp"

Cluster *cluster;
EventManager events;


double new_heuristic_dynamic(graph_t *graph, Cluster *cluster, int algoNum, bool isHeft) {
    algoNum = isHeft ? 1 : algoNum;
    vector<pair<vertex_t *, double>> ranks = calculateBottomLevels(graph, algoNum);
    removeSourceAndTarget(graph, ranks);
    sort(ranks.begin(), ranks.end(),
         [](pair<vertex_t *, double> a, pair<vertex_t *, double> b) {
             return a.second > b.second;

         });

    vertex_t *vertex = graph->first_vertex;
    while (vertex != nullptr) {
        if (vertex->in_degree == 0) {
            std::vector<shared_ptr<Event> > pred, succ;
            events.insert(
                    make_shared<Event>(vertex, nullptr, eventType::OnTaskStart, cluster->getMemBiggestFreeProcessor(),
                                       0.0, 0.0, pred, succ, false, vertex->name + "-s"));
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

    // free its memory
    this->processor->availableMemory += task->actuallyUsedMemory;
    //set its status to finished
    thisTask->status = Status::Finished;
    string thisId = this->id;
    removeOurselfFromSuccessors(this);
    for (const shared_ptr<Event> &successor: this->successors) {
        // updates successors' fire time
        successor->actualTimeFire = this->actualTimeFire;
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
            vector<shared_ptr<Event>> newEvents;
            bestTentativeAssignment(childTask, bestModifiedProcs, bestProcessorToAssign, newEvents);

            for (const auto &item: newEvents) {
                events.insert(item);
                item->processor->addEvent(item);
            }
            childTask->status = Status::Scheduled;
        }
    }
}


void bestTentativeAssignment(vertex_t *vertex, vector<shared_ptr<Processor>> &bestModifiedProcs,
                             shared_ptr<Processor> &bestProcessorToAssign, vector<shared_ptr<Event>> newEvents) {
    double bestStartTime, bestFinishTime;
    int resultingVar;
    for (auto &[id, processor]: cluster->getProcessors()) {
        double finTime = -1, startTime = -1;
        double ftBefore = processor->readyTimeCompute;
        int resultingEvictionVariant = -1;
        auto ourModifiedProc = make_shared<Processor>(*processor);


        //checkIfPendingMemoryCorrect(ourModifiedProc);
        vector<shared_ptr<Processor>> modifiedProcs = tentativeAssignment(vertex, ourModifiedProc,
                                                                          resultingEvictionVariant, finTime,
                                                                          startTime);

        if (bestFinishTime > finTime) {
            //    cout<<"best acutalize "<<endl;
            assert(!modifiedProcs.empty());
            bestModifiedProcs = modifiedProcs;
            bestFinishTime = finTime;
            bestStartTime = startTime;
            bestProcessorToAssign = ourModifiedProc;
            resultingVar = resultingEvictionVariant;
        }
    }
}

vector<shared_ptr<Processor>>
tentativeAssignment(vertex_t *vertex, shared_ptr<Processor> ourModifiedProc, int resultingEvictionVariant,
                    double finTime, double startTime) {
    vector<std::shared_ptr<Processor>> modifiedProcs;
    double earliestStartToCompute = 0;
    shared_ptr<Event> ourEvent;
    vector<shared_ptr<Event>> createdEvents;


    processIncomingEdges(vertex, ourEvent, ourModifiedProc, modifiedProcs, createdEvents, earliestStartToCompute);

}


void
processIncomingEdges(const vertex_t *v, shared_ptr<Event> ourEvent, shared_ptr<Processor> &ourModifiedProc,
                     vector<std::shared_ptr<Processor>> &modifiedProcs,
                     vector<shared_ptr<Event>> createdEvents, double startTimeOfTask) {
    for (int j = 0; j < v->in_degree; j++) {
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
    eventStartRead->predecessors.emplace_back(ourModifiedProc->lastReadEvent);

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
    return {eventStartRead, eventFinishRead};
}

vector<shared_ptr<Event>> evictFilesUntilThisFits(shared_ptr<Processor> thisProc, double weightToFit) {
    shared_ptr<Event> previouslyEvicted = nullptr;
    vector<shared_ptr<Event>> newEvents;
    while (thisProc->availableMemory < weightToFit) {
        edge_t *edgeToEvict = *thisProc->pendingMemories.begin();
        vector<shared_ptr<Event>> pred, succ = vector<shared_ptr<Event>>{};
        auto eventStartWrite = make_shared<Event>(nullptr, edgeToEvict, OnWriteStart, thisProc,
                                                  thisProc->readyTimeWrite, thisProc->readyTimeWrite,
                                                  pred,
                                                  succ, false,
                                                  buildEdgeName(edgeToEvict) + "-w-s");
        if (previouslyEvicted != nullptr) {
            eventStartWrite->predecessors.emplace_back(previouslyEvicted);
            previouslyEvicted->successors.emplace_back(eventStartWrite);
        }

        double timeFinishWrite = thisProc->readyTimeWrite + edgeToEvict->weight / thisProc->writeSpeedDisk;
        auto eventFinishWrite = make_shared<Event>(nullptr, edgeToEvict, OnWriteStart, thisProc,
                                                   timeFinishWrite,
                                                   timeFinishWrite,
                                                   pred,
                                                   succ, false,
                                                   buildEdgeName(edgeToEvict) + "-w-f");

        eventFinishWrite->predecessors.emplace_back(eventStartWrite);
        previouslyEvicted = eventFinishWrite;
        thisProc->readyTimeWrite = timeFinishWrite;
        thisProc->lastWriteEvent = eventFinishWrite;
        thisProc->availableMemory -= edgeToEvict->weight;
    }
    return newEvents;
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

vector<vertex_t *> getReadyTasks(graph_t *graph) {
    vector<vertex_t *> res;
    vertex_t *vertex = graph->first_vertex;
    while (vertex != nullptr) {
        bool isReady = true;
        for (int i = 0; i < vertex->in_degree; i++) {
            if (vertex->in_edges[i]->tail->status != Status::Finished) {
                isReady = false;
                break;
            }
        }
        if (isReady) {
            res.emplace_back(vertex);
        }
        vertex = vertex->next;
    }
    return res;
}


void onTaskFinish(Event event) {

    assert(event.type == eventType::OnTaskFinish);
    assert(event.task != NULL);

    //free memory - not being done

    //incoming edges are nowhere
    for (int j = 0; j < event.task->in_degree; j++) {
        event.task->in_edges[j]->locations = {Location(LocationType::Nowhere)};
    }
    //set event to ready
    event.task->status = Status::Finished;

    for (auto &successor: event.successors) {
        successor->predecessors.erase(
                std::remove_if(
                        successor->predecessors.begin(),
                        successor->predecessors.end(),
                        [event](const shared_ptr<Event> event1) {
                            return event1->task->name == event.task->name &&
                                   event1->expectedTimeFire == event.expectedTimeFire;
                        }
                ),
                successor->predecessors.end());
    }


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
    }
}

vector<Event> tryScheduleTask(vertex_t *task) {

}

double deviation(double in) {
    return in; //in* 2;
}