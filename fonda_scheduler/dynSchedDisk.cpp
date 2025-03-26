
#include <queue>
#include "fonda_scheduler/dynSchedDisk.hpp"
#include "fonda_scheduler/dynSched.hpp"
#include <random>

Cluster *cluster;
EventManager events;
ReadyQueue readyQueue;

string lastEventName;

double new_heuristic_dynamic(graph_t *graph, Cluster *cluster1, int algoNum, bool isHeft) {
    double resMakespan = -1;
    cluster = cluster1;
    algoNum = isHeft ? 1 : algoNum;
    enforce_single_source_and_target_with_minimal_weights(graph);
    compute_bottom_and_top_levels(graph);

    static thread_local std::mt19937 gen(std::random_device{}());
    static thread_local std::uniform_real_distribution<double> dist(0.0, 1);

    vertex_t *vertex = graph->first_vertex;
    switch (algoNum) {
        case 1: {
            vertex = graph->first_vertex;

            while(vertex!= nullptr){
                double rank = calculateSimpleBottomUpRank(vertex);
                rank = rank +  dist(gen);
                vertex->rank= rank;
                vertex = vertex->next;
            }
            break;
        }
        case 2: {
            vertex_t *vertex = graph->first_vertex;
            static thread_local std::mt19937 gen(std::random_device{}());
            static thread_local std::uniform_real_distribution<double> dist(0.0, 1);

            while(vertex!= nullptr){
                double rank = calculateBLCBottomUpRank(vertex);
                rank = rank +  dist(gen);
                vertex->rank= rank;
                vertex = vertex->next;
            }
            break;
        }
        case 3: {
            vector<std::pair<vertex_t *, double>> ranks = calculateMMBottomUpRank(graph);
            std::for_each(ranks.begin(), ranks.end(),[](std::pair<vertex_t *, double> pair){
                pair.first->rank= pair.second+  dist(gen);
            });
        }
            break;
        default:
            throw runtime_error("unknon algorithm");
    }


    vertex = graph->first_vertex;
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
        cntr++;
        shared_ptr<Event> firstEvent = events.getEarliest();
        bool removed = events.remove(firstEvent->id);
        assert(removed == true);

        if (firstEvent->id == lastEventName && !firstEvent->predecessors.empty()) {
            cout << "FIRE SAME EVENT " << lastEventName << " with predecessor "
                 << (*firstEvent->predecessors.begin())->id << endl;
            events.insert(firstEvent);
            events.update(firstEvent->id, firstEvent->getActualTimeFire() + 1);

            while (!firstEvent->predecessors.empty()) {
                firstEvent = *firstEvent->predecessors.begin();
            }
            cout << "firing predecessor without predecessors " << firstEvent->id << endl;
            removed = events.remove(firstEvent->id);
            assert(removed == true);
        }
        cout << "\nevent " << firstEvent->id << " at " << firstEvent->getActualTimeFire();
        cout << " num fired " << firstEvent->timesFired << endl;
        if (firstEvent->timesFired > 0 && !firstEvent->predecessors.empty()) {
            bool hasCycle = firstEvent->checkCycleFromEvent();
            assert(!hasCycle);
            events.insert(firstEvent);
            while (!firstEvent->predecessors.empty()) {
                firstEvent = *firstEvent->predecessors.begin();
            }
            cout << "firing predecessor without predecessors " << firstEvent->id << endl;
            removed = events.remove(firstEvent->id);
            assert(removed == true);
        }
        if (firstEvent->timesFired > 20) {
            bool hasCycle = firstEvent->checkCycleFromEvent();
            assert(!hasCycle);
        }

        firstEvent->fire();
        resMakespan = max(resMakespan, firstEvent->getActualTimeFire());
        lastEventName = firstEvent->id;
        //cout<<"events now "; events.printAll();
    }
    return resMakespan;
}

void Event::fireTaskStart() {
    string thisid = this->id;
    cout << "task start for " << thisid << " at " << this->actualTimeFire << " on proc " << this->processor->id << " ";
    double timeStart = 0;

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        cout << "BAD";
        //this->setActualTimeFire(
        //       this->getActualTimeFire()  +std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        events.insert(shared_from_this());
    } else {
        cout << "DONE";
        removeOurselfFromSuccessors(this);
        this->task->status = Status::Running;
        auto ourFinishEvent = events.find(this->task->name + "-f");
        if (ourFinishEvent == nullptr) {
            throw runtime_error("NOt found finish event to " + this->task->name);
        }
        double durationTask = ourFinishEvent->getExpectedTimeFire() - this->getExpectedTimeFire();
        assert(this->task->name == "GRAPH_SOURCE" ||
               durationTask >= this->task->time / this->processor->getProcessorSpeed()
               || abs(durationTask - this->task->time / this->processor->getProcessorSpeed()) < 0.1
        );


        double factor = applyDeviationTo(durationTask);
        this->task->factorForRealExecution = factor;
        assert(factor >0);
        for (auto successor = successors.begin(); successor != successors.end();) {
            // cout << " from " << (*successor)->id << "'s predecessors" << endl;
            if ((*successor)->task == nullptr) {
                throw runtime_error("edge-based event depends on task start " + this->id);
            }
            successor++;
        }

        double d = this->getActualTimeFire() + durationTask;
        // cout << "on start  setting finish time from "<< ourFinishEvent->actualTimeFire <<" to " << d << endl;
        //ourFinishEvent->setActualTimeFire(d);
        events.update(ourFinishEvent->id, d);
        this->isDone = true;

        //propagateChainActualTimeIfSmaller(ourFinishEvent);

    }
    cout << endl;
}


void Event::fireTaskFinish() {
    vertex_t *thisTask = this->task;
    cout << "firing task Finish for " << this->id;
    if(this->id=="bismark_report_00000008-f"){
        cout<<endl;
    }

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        cout << "BAD " << endl;
        events.insert(shared_from_this());
    } else {
        cout << "DONE ";
        removeOurselfFromSuccessors(this);
        //set its status to finished
        this->task->status = Status::Finished;
        this->isDone = true;
        this->task->makespan = this->actualTimeFire;

        assert(this->processor->getAvailableMemory() <= this->processor->getMemorySize());


        string thisId = this->id;

        for (int i = 0; i < thisTask->out_degree; i++) {
            locateToThisProcessorFromNowhere(thisTask->out_edges[i], this->processor->id);
        }

        for (int i = 0; i < thisTask->out_degree; i++) {
            vertex_t *childTask = thisTask->out_edges[i]->head;
            //cout << "deal with child " << childTask->name << endl;
            if(childTask->name=="preseq_00000007"){
                cout<<endl;
            }
            bool isReady = true;
            for (int j = 0; j < childTask->in_degree; j++) {
                if (childTask->in_edges[j]->tail->status == Status::Unscheduled) {
                    isReady = false;
                }
            }
           

            std::vector<shared_ptr<Event> > pred, succ;
           
            if (isReady && childTask->status != Status::Scheduled) {
               // cout<<"inserting into ready "<<childTask->name<<endl;
                readyQueue.readyTasks.insert(childTask);
              
            }
        }

        this->isDone = true;

        bool foundSomeTaskForOurProcessor = false;

        while(!foundSomeTaskForOurProcessor && !readyQueue.readyTasks.empty()){
            vertex_t *mostReadyVertex = *readyQueue.readyTasks.begin();


            vector<shared_ptr<Processor>> bestModifiedProcs;
            shared_ptr<Processor> bestProcessorToAssign;
            vector<shared_ptr<Event>> newEvents =
                    bestTentativeAssignment(mostReadyVertex, bestModifiedProcs, bestProcessorToAssign,
                                            this->actualTimeFire);

            for (const auto &item: newEvents) {
                events.insert(item);
                item->processor->addEvent(item);
            }
            mostReadyVertex->status = Status::Scheduled;
            readyQueue.readyTasks.erase(mostReadyVertex);

            if(bestProcessorToAssign->id == this->processor->id){
                foundSomeTaskForOurProcessor= true;
            }
        }
        


    }
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
    cout << "firing read start for " << this->id << " ";
    // assert(finishRead->getActualTimeFire()> this->getActualTimeFire());
    auto canRun = dealWithPredecessors(shared_from_this());

    if (!canRun) {
        cout << "BAD " << (*this->predecessors.begin())->id << endl;
        events.insert(shared_from_this());
    } else {
        cout << "DONE";
        removeOurselfFromSuccessors(this);
        double durationOfRead = this->edge->weight / this->processor->readSpeedDisk;
        double factor = applyDeviationTo(durationOfRead);
        assert(factor>0);
        this->edge->factorForRealExecution= factor;
        double expectedTimeFireFinish = this->actualTimeFire + durationOfRead;

        this->isDone = true;

        shared_ptr<Event> finishRead = events.find(buildEdgeName(this->edge) + "-r-f");
        if (finishRead == nullptr) {
            throw runtime_error("NO read finish found for " + this->id);
        } else {
            events.update(buildEdgeName(this->edge) + "-w-f", expectedTimeFireFinish);
        }
    }
    cout << endl;

}

void Event::fireReadFinish() {
    cout << "firing read finish for " << this->id << endl;

    shared_ptr<Event> startRead = events.find(buildEdgeName(this->edge) + "-r-s");

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        cout << "BAD because " << (*this->predecessors.begin())->id << endl;
        events.insert(shared_from_this());
    } else {
        cout << "DONE";
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
    cout << "firing write start for " << this->id << endl;
    if (buildEdgeName(this->edge) == "GRAPH_SOURCE-CHECK_DESIGN" ||
        buildEdgeName(this->edge) == "GRAPH_SOURCE-MAKE_GENOME_FILTER") {
        cout << endl;
    }
    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        cout << "BAD" << (*this->predecessors.begin())->id << endl;
        events.insert(shared_from_this());
    } else {
        cout << "DONE" << endl;
        removeOurselfFromSuccessors(this);

        double durationOfWrite = this->edge->weight / this->processor->writeSpeedDisk;
        double factor = applyDeviationTo(durationOfWrite);
        this->edge->factorForRealExecution = factor;
        assert(factor>0);
        double actualTimeFireFinish = this->actualTimeFire + durationOfWrite;
        this->isDone = true;
        shared_ptr<Event> finishWrite =
                events.find(buildEdgeName(this->edge) + "-w-f");
        if (finishWrite == nullptr) {
            throw runtime_error("NO write finish found for " + this->id);
        } else {

            events.update(buildEdgeName(this->edge) + "-w-f", actualTimeFireFinish);
            assert(!finishWrite->checkCycleFromEvent());
        }

    }
}

void Event::fireWriteFinish() {
    cout << "firing write finish for " << this->id << endl;
    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        cout << "BAD " << (*this->predecessors.begin())->id << endl;
        events.insert(shared_from_this());
    } else {
        cout << "DONE";
        removeOurselfFromSuccessors(this);
        delocateFromThisProcessorToDisk(this->edge, this->processor->id);
        this->isDone = true;
    }
}

void Event::removeOurselfFromSuccessors(Event *us) {
    // cout << "removing from successors us, " << us->id;

    for (auto successor = successors.begin(); successor != successors.end();) {
        //      cout << " from " << (*successor)->id << "'s predecessors" << endl;
//
        for (auto succspred = (*successor)->predecessors.begin(); succspred != (*successor)->predecessors.end();) {
            if ((*succspred)->id == us->id) {
                //  cout << "removed" << endl;
                succspred = (*successor)->predecessors.erase(succspred);
            } else {
                // Move to the next element
                ++succspred;
            }

        }
        successor++;
    }
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
    /*auto foundIterator = this->eventsOnProc.find(event.get()->id);
    if (foundIterator != eventsOnProc.end() && !foundIterator->second.expired()) {
        //  cout << "event already exists on Processor " << this->id << endl;
        if (event->getActualTimeFire() != foundIterator->second.lock()->getActualTimeFire()) {
            //         cout << "Updating time fire " << endl;
            foundIterator->second.lock()->setBothTimesFire(event->getActualTimeFire());
        }
    } else {
        this->eventsOnProc[event->id] = event;
    } */
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
                //  cout<<"predecessor "<<item->id<<"'s fire time is larger than ours. "<<item->getActualTimeFire()<<" vs "<<us->getActualTimeFire()<<endl;
                us->setActualTimeFire(
                        item->getActualTimeFire());

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


double applyDeviationTo(double &in) {
    // in = in * 2;
    // return 2;
    // return 1;
    static std::random_device rd;
    static std::mt19937 gen(rd());  // Mersenne Twister PRNG

    double stddev = in * 0.1;  // 10% of the input value
    std::normal_distribution<double> dist(in, stddev);

    double result = dist(gen);
    double factor = result / in;
    in = result;
    return factor;

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

double Processor::getReadyTimeCompute() {
    if (!this->lastComputeEvent.expired() &&
        this->lastComputeEvent.lock()->getActualTimeFire() != this->readyTimeCompute) {
        this->readyTimeCompute = lastComputeEvent.lock()->getActualTimeFire();
    }
    return this->readyTimeCompute;
}

double Processor::getReadyTimeWrite() {
    if (!this->lastWriteEvent.expired() && this->lastWriteEvent.lock()->getActualTimeFire() != this->readyTimeWrite) {
        this->readyTimeWrite = lastWriteEvent.lock()->getActualTimeFire();
    }
    return this->readyTimeWrite;
}

double Processor::getReadyTimeRead() {
    if (!this->lastReadEvent.expired() && this->lastReadEvent.lock()->getActualTimeFire() != this->readyTimeRead) {
        this->readyTimeRead = lastReadEvent.lock()->getActualTimeFire();
    }
    return this->readyTimeRead;
}


double Processor::getExpectedOrActualReadyTimeCompute() {
    if (!this->lastComputeEvent.expired() ) {
        if(this->lastComputeEvent.lock()->isDone){
            return this->lastComputeEvent.lock()->getActualTimeFire();
        }
        else{
            return lastComputeEvent.lock()->getExpectedTimeFire();
        }
    }
    else return this->readyTimeCompute;

}

double Processor::getExpectedOrActualReadyTimeWrite() {
    if (!this->lastWriteEvent.expired() ) {
        if(this->lastWriteEvent.lock()->isDone){
            return this->lastWriteEvent.lock()->getActualTimeFire();
        }
        else{
            return lastWriteEvent.lock()->getExpectedTimeFire();
        }
    }
    else return  this->readyTimeWrite;
}

double Processor::getExpectedOrActualReadyTimeRead() {
    if (!this->lastReadEvent.expired() ) {
        if(this->lastReadEvent.lock()->isDone){
            return this->lastReadEvent.lock()->getActualTimeFire();
        }
        else{
            return lastReadEvent.lock()->getExpectedTimeFire();
        }
    }
    else return this->readyTimeRead;
}