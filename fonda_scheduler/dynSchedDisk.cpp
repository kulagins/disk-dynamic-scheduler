
#include <queue>
#include "fonda_scheduler/dynSchedDisk.hpp"
#include "fonda_scheduler/dynSched.hpp"

Cluster *cluster;
EventManager events;



double new_heuristic_dynamic(graph_t *graph, Cluster *cluster1, int algoNum, bool isHeft) {
    double resMakespan = -1;
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
  //  cout << "task start for " << thisid << " at " << this->actualTimeFire << " on proc " << this->processor->id << endl;
    double timeStart = 0;

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        this->setActualTimeFire(
                this->getActualTimeFire() + +std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        events.insert(shared_from_this());
    } else {
        removeOurselfFromSuccessors(this);
        this->task->status = Status::Running;
        auto ourFinishEvent = events.find(this->task->name + "-f");
        if (ourFinishEvent == nullptr) {
            throw runtime_error("NOt found finish event to " + this->task->name);
        }
        assert(this->task->name=="GRAPH_SOURCE"  ||
                ourFinishEvent->getExpectedTimeFire() - this->getExpectedTimeFire()>= this->task->time/ this->processor->getProcessorSpeed()
                || abs(ourFinishEvent->getExpectedTimeFire() - this->getExpectedTimeFire() - this->task->time/ this->processor->getProcessorSpeed()) <0.1
        );

        double actualRuntime = deviation(ourFinishEvent->getExpectedTimeFire() - this->getExpectedTimeFire());

        double d = this->getActualTimeFire() + actualRuntime;
        // cout << "on start  setting finish time from "<< ourFinishEvent->actualTimeFire <<" to " << d << endl;
        ourFinishEvent->setActualTimeFire(d);
        this->isDone = true;
    }
}


void Event::fireTaskFinish() {
    vertex_t *thisTask = this->task;
  //  cout << "firing task Finish for " << this->id << endl;

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        events.insert(shared_from_this());
    } else {
        removeOurselfFromSuccessors(this);
        //set its status to finished
        this->task->status = Status::Finished;
        this->isDone = true;
        this->task->makespan = this->actualTimeFire;

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
                        bestTentativeAssignment(childTask, bestModifiedProcs, bestProcessorToAssign,
                                                this->actualTimeFire);

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
        assert(events.find(this->predecessors.at(0)->id) != nullptr);
        this->setActualTimeFire(
                this->getActualTimeFire() + +std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        assert(this->actualTimeFire >= this->predecessors.at(0)->actualTimeFire);
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
        this->setActualTimeFire(
                this->getActualTimeFire() + +std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        events.insert(shared_from_this());
    } else {
        removeOurselfFromSuccessors(this);
        if (!isLocatedOnDisk(this->edge)) {
            auto ptr = events.find(buildEdgeName(this->edge) + "-w-f");
            assert(ptr != nullptr);
            auto ptr1 = events.find(buildEdgeName(this->edge) + "-r-s");
            assert(ptr->getActualTimeFire() < this->getActualTimeFire());
        }
        locateToThisProcessorFromDisk(this->edge, this->processor->id);
        this->isDone = true;
    }
}

void Event::fireWriteStart() {
   // cout << "firing write start for ";
  //  print_edge(this->edge);
    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        this->setActualTimeFire(
                this->getActualTimeFire() + +std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
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
        this->setActualTimeFire(
                this->getActualTimeFire() + +std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
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
        double prev  = us->getActualTimeFire();
        // cout << "predecessors not empty for " << us->id << endl;
        for (const auto &item: us->predecessors) {
            //      cout << "predecessor " << item->id << ", ";
            if (item->getActualTimeFire() > us->getActualTimeFire()) {
                if(abs(item->getActualTimeFire() - us->getActualTimeFire())>0.1){
                    us->setActualTimeFire(item->getActualTimeFire());
                }
            }
        }
        //   cout << endl;
        double diff = us->getActualTimeFire()- prev;

        if(diff>0){
            for (const auto &item: us->successors) {
                //TODO TABLE
                item->setActualTimeFire(max(item->getActualTimeFire(), us->getActualTimeFire()));
            }
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


void Processor::setLastWriteEvent(shared_ptr<Event> lwe) {
    this->lastWriteEvent = lwe;
    this->readyTimeWrite = lwe->getActualTimeFire();
}

void Processor::setLastReadEvent(shared_ptr<Event> lre) {
    this->lastReadEvent = lre;
    this->readyTimeRead = lre->getActualTimeFire();
}

void Processor::setLastComputeEvent(shared_ptr<Event> lce) {
    this->lastComputeEvent = lce;
    this->readyTimeCompute = lce->getActualTimeFire();
}