
#include <queue>
#include "fonda_scheduler/DynamicSchedulerHeader.hpp"
#include "fonda_scheduler/SchedulerHeader.hpp"
#include <random>

Cluster *cluster;
EventManager events;
ReadyQueue readyQueue;
int devationVariant;
bool usePreemptiveWrites;

string lastEventName;

double new_heuristic_dynamic(graph_t *graph, Cluster *cluster1, int algoNum, bool isHeft, int deviationNumber, bool upw) {
    double resMakespan = -1;
    cluster = cluster1;
    algoNum = isHeft ? 1 : algoNum;
    enforce_single_source_and_target_with_minimal_weights(graph);
    compute_bottom_and_top_levels(graph);
    devationVariant = deviationNumber;
    usePreemptiveWrites= upw;

    static thread_local std::mt19937 gen(std::random_device{}());
    static thread_local std::uniform_real_distribution<double> dist(0.0, 1);


    vertex_t *vertex = graph->first_vertex;
    switch (algoNum) {
        case 1: {
            vertex = graph->first_vertex;

            while (vertex != nullptr) {
                double rank = calculateSimpleBottomUpRank(vertex);
                rank = rank + dist(gen);
                vertex->rank = rank;
                vertex = vertex->next;
            }
            break;
        }
        case 2: {
            vertex_t *vertex = graph->first_vertex;
            static thread_local std::mt19937 gen(std::random_device{}());
            static thread_local std::uniform_real_distribution<double> dist(0.0, 1);

            while (vertex != nullptr) {
                double rank = calculateBLCBottomUpRank(vertex);
                rank = rank + dist(gen);
                vertex->rank = rank;
                vertex = vertex->next;
            }
            break;
        }
        case 3: {
            vector<std::pair<vertex_t *, double>> ranks = calculateMMBottomUpRank(graph);
            std::for_each(ranks.begin(), ranks.end(), [](std::pair<vertex_t *, double> pair) {
                pair.first->rank = pair.second + dist(gen);
            });
        }
            break;
        default:
            throw runtime_error("unknon algorithm");
    }

    if (findVertexByName(graph, "GRAPH_SOURCE") != nullptr) {
        remove_vertex(graph, findVertexByName(graph, "GRAPH_SOURCE"));
        remove_vertex(graph, findVertexByName(graph, "GRAPH_TARGET"));
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

        if (firstEvent->id == lastEventName && !firstEvent->getPredecessors().empty()) {
             //cout << "FIRE SAME EVENT " << lastEventName << " with predecessor "
               //   << (*firstEvent->getPredecessors().begin())->id << endl;
            events.insert(firstEvent);
            events.update(firstEvent->id, firstEvent->getActualTimeFire() + 1);

            while (!firstEvent->getPredecessors().empty() && !(*firstEvent->getPredecessors().begin())->isDone) {
                firstEvent = *firstEvent->getPredecessors().begin();
            }
           // cout << "firing predecessor without predecessors " << firstEvent->id <<" done? "<<(firstEvent->isDone?"yes":"no") << endl;
            removed = events.remove(firstEvent->id);
            // assert(removed == true);
        }
        // cout << "\nevent " << firstEvent->id << " at " << firstEvent->getActualTimeFire();
        //   cout << " num fired " << firstEvent->timesFired << endl;
        if (firstEvent->timesFired > 0 && !firstEvent->getPredecessors().empty()) {
          // cout << "2-FIRE SAME EVENT " << lastEventName << " with predecessor "
          //         << (*firstEvent->getPredecessors().begin())->id << endl;
            bool hasCycle = firstEvent->checkCycleFromEvent();
            assert(!hasCycle);
            events.insert(firstEvent);
            while (!firstEvent->getPredecessors().empty() && !(*firstEvent->getPredecessors().begin())->isDone) {
                firstEvent = *firstEvent->getPredecessors().begin();
            }
            //cout << "2-firing predecessor without predecessors " << firstEvent->id <<" done? "<<(firstEvent->isDone?"yes":"no") << endl;
            removed = events.remove(firstEvent->id);
            //assert(removed == true);
        }


        if (removed || !firstEvent->isDone) {
          //  cout<<"finally event "<<firstEvent->id<<endl;
            firstEvent->fire();
            resMakespan = max(resMakespan, firstEvent->getActualTimeFire());
            lastEventName = firstEvent->id;
            auto ptr = events.findByEventId(firstEvent->id);
        } else {
            cout << "nthng " << endl;
            return -1;
        }
//        assert(ptr == nullptr);

        //  cout<<"events now "; events.printAll();
    }
    return resMakespan;
}

void Event::fireTaskStart() {
    string thisid = this->id;
   // cout << " firing task start for " << thisid << " at " << this->actualTimeFire << " on proc " << this->processor->id << endl;

    //   cout<<this->task->name<<" "<<this->actualTimeFire<<" "<<events.find(this->task->name+"-f")->getActualTimeFire()<<" on "<<this->processor->id<<endl;

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
       // cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
       //      << endl;
        //this->setActualTimeFire(
        //       this->getActualTimeFire()  +std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        events.insert(shared_from_this());
    } else {
        //cout << "DONE" << endl;
        removeOurselfFromSuccessors(this);
        this->task->status = Status::Running;
        auto ourFinishEvent = events.findByEventId(this->task->name + "-f");
        if (ourFinishEvent == nullptr) {
            throw runtime_error("NOt found finish event to " + this->task->name);
        }
        double durationTask = ourFinishEvent->getExpectedTimeFire() - this->getExpectedTimeFire();
        assert(durationTask>0);
        assert(this->task->name == "GRAPH_SOURCE" ||
               durationTask >= this->task->time / this->processor->getProcessorSpeed()
               || abs(durationTask - this->task->time / this->processor->getProcessorSpeed()) < 0.1
        );


        double factor = applyDeviationTo(durationTask);
        this->task->factorForRealExecution = factor;
        assert(factor > 0);
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

        for (int i = 0; i < this->task->in_degree; i++) {
            edge_t *inEdge   = this->task->in_edges[i];
            const string &edgeName = buildEdgeName(inEdge);
            shared_ptr<Event> startWrite = events.findByEventId(edgeName + "-w-s");
            shared_ptr<Event> finishWrite = events.findByEventId(buildEdgeName(inEdge) + "-w-f");
            if (startWrite != nullptr) {
                events.remove(startWrite->id);
                events.remove(finishWrite->id);
                removeOurselfFromSuccessors(startWrite.get());
                removeOurselfFromSuccessors(finishWrite.get());
                startWrite->isDone=true;
                finishWrite->isDone=true;


                unordered_set<Event*> visited;
                propagateChainInPlanning(finishWrite,
                                         startWrite->getActualTimeFire() - finishWrite->getActualTimeFire(), visited);
            } else {
                if (finishWrite != nullptr) {
                    finishWrite->setActualTimeFire(this->getActualTimeFire());
                }
            }

            for (auto &item: cluster->getProcessors()){

                if(!item.second->writingQueue.empty() &&
                        std::find(item.second->writingQueue.begin(), item.second->writingQueue.end(),inEdge)!= item.second->writingQueue.end()){
                   // cout<<"erasing in edges of"<< buildEdgeName(inEdge)<<" from wiriting queueu of proc "<<item.first<<endl;
                    item.second->writingQueue.erase(std::find(item.second->writingQueue.begin(), item.second->writingQueue.end(),inEdge));
                }
            }
        }

    }
    //cout << endl;
}


void Event::fireTaskFinish() {
    vertex_t *thisTask = this->task;
  //  cout << "firing task Finish for " << this->id << " at " << this->getActualTimeFire() << endl;
    //  cout<<this->actualTimeFire<<" on "<<this->processor->id<<endl;

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
   //     cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
      //       << endl;
        events.insert(shared_from_this());
    } else {
        //cout << "DONE " << endl;
        removeOurselfFromSuccessors(this);
        //set its status to finished
        this->task->status = Status::Finished;
        this->isDone = true;
        this->task->makespan = this->actualTimeFire;

        assert(cluster->getProcessorById(this->processor->id).use_count() ==
               this->processor.use_count());

        assert(this->processor->getAvailableMemory() <= this->processor->getMemorySize());


        string thisId = this->id;

        for (int i = 0; i < thisTask->out_degree; i++) {
            locateToThisProcessorFromNowhere(thisTask->out_edges[i], this->processor->id, false,
                                             this->getActualTimeFire());
        }

        for (int i = 0; i < thisTask->out_degree; i++) {
            vertex_t *childTask = thisTask->out_edges[i]->head;
            //cout << "deal with child " << childTask->name << endl;
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

            if(usePreemptiveWrites){
                cluster->getProcessorById(this->processor->id)->writingQueue.emplace_back(thisTask->out_edges[i]);
            }

        }


        this->isDone = true;

        bool foundSomeTaskForOurProcessor = false;

        bool existsIdleProcessor = false;

        for (const auto &item: cluster->getProcessors()) {
            if (item.second->getReadyTimeCompute() < this->getActualTimeFire()) {
                existsIdleProcessor = true;
                break;
            }
        }

        while ((!foundSomeTaskForOurProcessor || existsIdleProcessor) && !readyQueue.readyTasks.empty()) {
            vertex_t *mostReadyVertex = *readyQueue.readyTasks.begin();
            vector<shared_ptr<Processor>> bestModifiedProcs;
            shared_ptr<Processor> bestProcessorToAssign;
            // cout<<"assigning vertex "<<mostReadyVertex->name<<" ";
            vector<shared_ptr<Event>> newEvents =
                    bestTentativeAssignment(mostReadyVertex, bestModifiedProcs, bestProcessorToAssign,
                                            this->actualTimeFire);


            mostReadyVertex->status = Status::Scheduled;
            readyQueue.readyTasks.erase(mostReadyVertex);

            if (bestProcessorToAssign->id == this->processor->id) {
                foundSomeTaskForOurProcessor = true;
            }

            for (auto &item: bestModifiedProcs) {
                //  cout<<item.use_count()<<" ";
                item.reset();
            }
            //  cout<<endl;
            // cout<<bestProcessorToAssign.use_count()<<endl;
            bestProcessorToAssign.reset();

            for (const auto &item: newEvents) {
                assert(cluster->getProcessorById(item->processor->id).use_count() ==
                       item->processor.use_count());
                events.insert(item);
               // cout<<"new event "<<item->id<<" w preds "<<endl;
                //for (const auto &item1: item->predecessors){
                //    cout<<item1->id<<", ";
                //}
                // cout <<endl<<"successors ";
                //for (const auto &item1: item->successors){
                //     cout<<item1->id<<", ";
                // }
                //  cout <<endl;
            }


            existsIdleProcessor = false;
            for (const auto &item: cluster->getProcessors()) {
                if (item.second->getReadyTimeCompute() < this->getActualTimeFire()) {
                    existsIdleProcessor = true;
                    break;
                }
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
        // cout << "adding modified proc " << addedProc->id << endl;
        modifiedProcs.emplace_back(addedProc);
        //checkIfPendingMemoryCorrect(addedProc);
    } else {
        addedProc = *it;
    }
    return addedProc;
}


void Event::fireReadStart() {
   // cout << "firing read start for " << this->id << " at " << this->actualTimeFire << endl;
    // assert(finishRead->getActualTimeFire()> this->getActualTimeFire());
    auto canRun = dealWithPredecessors(shared_from_this());

    if (!canRun) {
      //  cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
       //      << endl;
        events.insert(shared_from_this());
    } else {
        //cout << "DONE" << endl;
        removeOurselfFromSuccessors(this);
        double durationOfRead = this->edge->weight / this->processor->readSpeedDisk;
        double factor = applyDeviationTo(durationOfRead);
        assert(factor > 0);
        this->edge->factorForRealExecution = factor;
        double expectedTimeFireFinish = this->actualTimeFire + durationOfRead;

        this->isDone = true;

        shared_ptr<Event> finishRead = events.findByEventId(buildEdgeName(this->edge) + "-r-f");
        if (finishRead == nullptr) {
            throw runtime_error("NO read finish found for " + this->id);
        } else {
            events.update(buildEdgeName(this->edge) + "-w-f", expectedTimeFireFinish);
        }
    }
    //  cout << endl;

}

void Event::fireReadFinish() {
  //  cout << "firing read finish for " << this->id << " at " << this->getActualTimeFire() << " on "
   //      << this->processor->id << endl;

    shared_ptr<Event> startRead = events.findByEventId(buildEdgeName(this->edge) + "-r-s");

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
      //  cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
         //    << endl;
        events.insert(shared_from_this());
    } else {
        //cout << "DONE " << endl;
        removeOurselfFromSuccessors(this);
        assert(cluster->getProcessorById(this->processor->id).use_count() ==
               this->processor.use_count());
        if (!isLocatedOnDisk(this->edge, false)) {
            auto ptr = events.findByEventId(buildEdgeName(this->edge) + "-w-f");
            assert(ptr != nullptr);
            auto ptr1 = events.findByEventId(buildEdgeName(this->edge) + "-r-s");
            assert(ptr->getActualTimeFire() < this->getActualTimeFire());
        }
        locateToThisProcessorFromDisk(this->edge, this->processor->id, false, this->getActualTimeFire());
        this->isDone = true;
    }
}

void Event::fireWriteStart() {
   // cout << "firing write start for " << this->id << " at " << this->getActualTimeFire() << endl;

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
     //   cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
     //        << endl;
        events.insert(shared_from_this());
    } else {
        //cout << "DONE" << endl;
        removeOurselfFromSuccessors(this);

        assert(cluster->getProcessorById(this->processor->id).use_count() ==
               this->processor.use_count());

        double durationOfWrite = this->edge->weight / this->processor->writeSpeedDisk;
        double factor = applyDeviationTo(durationOfWrite);
        this->edge->factorForRealExecution = factor;
        assert(factor > 0);
        double actualTimeFireFinish = this->actualTimeFire + durationOfWrite;
        this->isDone = true;
        shared_ptr<Event> finishWrite =
                events.findByEventId(buildEdgeName(this->edge) + "-w-f");
        if (finishWrite == nullptr) {
            throw runtime_error("NO write finish found for " + this->id);
        } else {

            events.update(buildEdgeName(this->edge) + "-w-f", actualTimeFireFinish);
            assert(!finishWrite->checkCycleFromEvent());
        }

        string thisid = buildEdgeName(this->edge);
        auto edgeInWritingQueue = std::find(this->processor->writingQueue.begin(), this->processor->writingQueue.end(),
                                            this->edge);
        if (edgeInWritingQueue != this->processor->writingQueue.end()) {
            //cluster->getProcessorById(this->processor->id)->writingQueue.erase(edgeInWritingQueue);
            this->processor->writingQueue.erase(edgeInWritingQueue);
        }

    }
}

void Event::fireWriteFinish() {
  //  cout << "firing write finish for " << this->id << " at " << this->getActualTimeFire() << " on "
   //      << this->processor->id << endl;

    auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
       // cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
        //     << endl;
        events.insert(shared_from_this());
    } else {
        //cout << "DONE" << endl;
        removeOurselfFromSuccessors(this);
        if(this->onlyPreemptive){
            locateToDisk(this->edge,false, this->getActualTimeFire());
            assert(isLocatedOnThisProcessor(this->edge, this->processor->id, false));
        }
        else{
            delocateFromThisProcessorToDisk(this->edge, this->processor->id, false, this->getActualTimeFire());
        }
        assert(cluster->getProcessorById(this->processor->id).use_count() ==
               this->processor.use_count());
        auto positionInWriteQ = std::find(this->processor->writingQueue.begin(), this->processor->writingQueue.end(),
                                          this->edge);
        if(positionInWriteQ!= this->processor->writingQueue.end()){
            this->processor->writingQueue.erase(
                    positionInWriteQ);
        }
        else{
            cout<<"";
        }


        this->isDone = true;

        if(!this->processor->writingQueue.empty()){
            edge_t *edgeToWriteJustInCase = this->processor->writingQueue.at(0);
            if (events.findByEventId(buildEdgeName(edgeToWriteJustInCase) + "-w-s") == nullptr &&
                events.findByEventId(buildEdgeName(edgeToWriteJustInCase) + "-w-f") == nullptr
                    ) {

                double presumedLength = assessWritingOfEdge(edgeToWriteJustInCase, this->processor);
                double startOfNextWrite = std::numeric_limits<double>::max();
                const set<std::shared_ptr<Event>, CompareByTimestamp> eventsOnThisProc = events.findByProcessorId(
                        this->processor->id);
                assert((*eventsOnThisProc.begin())->getActualTimeFire() <=
                       (*eventsOnThisProc.rbegin())->getActualTimeFire());

                for (auto &item: eventsOnThisProc) {
                    //  cout<<"event on proc "<<item->id<<" at "<<item->getActualTimeFire()<<endl;
                    //events are ordered by timestamp, so iterating from earliest to latest
                    if (item->type == eventType::OnWriteStart &&
                        item->getActualTimeFire() > this->getActualTimeFire()) {
                        startOfNextWrite = item->getActualTimeFire();
                        break;
                    }
                }

                if (this->getActualTimeFire() + presumedLength < startOfNextWrite) {
                    // can fit
                   // cout << "scheduling extra write for " << buildEdgeName(edgeToWriteJustInCase) << endl;
                    assert(events.findByEventId(buildEdgeName(edgeToWriteJustInCase)+"-w-s")== nullptr);
                    assert(events.findByEventId(buildEdgeName(edgeToWriteJustInCase)+"-w-f")== nullptr);
                    std::pair<shared_ptr<Event>, shared_ptr<Event>> writeEvents;
                    auto iterator = scheduleWriteForEdge(this->processor, edgeToWriteJustInCase, writeEvents, true);
                    events.insert(writeEvents.first);
                    events.insert(writeEvents.second);
                    assert(isLocatedOnThisProcessor(edgeToWriteJustInCase, this->processor->id, false));
                    this->processor->writingQueue.erase(this->processor->writingQueue.begin());
                    if(buildEdgeName(edgeToWriteJustInCase)=="preseq_00000155-multiqc_00000149"){
                        cout<<endl;
                    }
                    // for (const auto &item: this->processor->writingQueue){
                    //    cout<<buildEdgeName(item)<<endl;
                    // }
                    //cout<<"----------"<<endl;
                    //for (const auto &item: cluster->getProcessorById(this->processor->id)->writingQueue){
                    //     cout<<buildEdgeName(item)<<endl;
                    //  }
                }
            } else {
                //cout << "event for " << buildEdgeName(edgeToWriteJustInCase) << " already in queue" << endl;
                this->processor->writingQueue.erase(this->processor->writingQueue.begin());

            }
        }

    }

}

void Event::removeOurselfFromSuccessors(Event *us) {
    // cout << "removing from successors us, " << us->id;

    for (auto successor = successors.begin(); successor != successors.end();) {
        //cout << " from " << (*successor)->id << "'s predecessors "; //<< endl;
        bool isREmoved = false;
        for (auto succspred = (*successor)->predecessors.begin(); succspred != (*successor)->predecessors.end();) {
            if ((*succspred)->id == us->id) {
                //cout << "removed" << endl;
                succspred = (*successor)->predecessors.erase(succspred);
                isREmoved = true;
            } else {
                // Move to the next element
                ++succspred;
            }

        }
       // if (!isREmoved) cout << "NOT REMOVED FROM SUCCESSOR " << (*successor)->id << endl;
        successor++;
    }
    // successors.clear();
    /*cout << "sanity check!" << endl;
    for (const auto &s: successors) {
        std::cout << "Successor " << s->id << " has predecessors: ";
        for (const auto &pred: s->predecessors) {
            std::cout << pred->id << " ";
        }
        std::cout << std::endl;
    } */
}


void Cluster::printProcessorsEvents() {

    /* for (const auto &[key, value]: this->processors) {
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

     } */
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
    if (!us->getPredecessors().empty()) {

        auto it = us->getPredecessors().begin();
        while (it != us->getPredecessors().end()) {
            if ((*it)->isDone) {
                it = us->getPredecessors().erase(it);
            } else {
                it++;
            }
        }
        // cout << "predecessors not empty for " << us->id << endl;
        for (const auto &item: us->getPredecessors()) {
            //      cout << "predecessor " << item->id << ", ";
            if (item->getActualTimeFire() > us->getActualTimeFire()) {
                //  cout<<"predecessor "<<item->id<<"'s fire time is larger than ours. "<<item->getActualTimeFire()<<" vs "<<us->getActualTimeFire()<<endl;
                us->setActualTimeFire(
                        item->getActualTimeFire());

            }
        }


    }
    return us->getPredecessors().empty();
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
    if (in == 0) {
        double result;
        switch (devationVariant) {
            case 1:
                result = 1.1;
            case 2:
                result =  1.5;
            case 3:
                result =  1;
            case 4:
                result =  2;
            case 5:
                result= 1.3;
            default:
                throw runtime_error("unknown deviation variant");
        }

        in = result;
        return result / 1.0;  // to preserve interface
    }
    static std::random_device rd;
    static std::mt19937 gen(rd());  // Mersenne Twister PRNG
    double result, factor, stddev;
    switch (devationVariant) {
        case 1:
            stddev = in * 0.1;
            break;
        case 2:
            stddev = in * 0.5;
            break;
        case 3:
            stddev = 0;
            break;
        case 4:
            stddev = 0;
            break;
        case 5:
            stddev = in * 0.3;
            break;
        default:
            throw runtime_error("unknown deviation variant");
    }

    std::normal_distribution<double> dist(in, stddev);
    result = dist(gen);
    result = max(result, 1.0);
    if (devationVariant == 4) {
        result *= 2;
    }
    factor = result / in;
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
    if (!this->lastComputeEvent.expired()) {
        if (this->lastComputeEvent.lock()->isDone) {
            return this->lastComputeEvent.lock()->getActualTimeFire();
        } else {
            return lastComputeEvent.lock()->getExpectedTimeFire();
        }
    } else return this->readyTimeCompute;

}

double Processor::getExpectedOrActualReadyTimeWrite() {
    if (!this->lastWriteEvent.expired()) {
        if (this->lastWriteEvent.lock()->isDone) {
            return this->lastWriteEvent.lock()->getActualTimeFire();
        } else {
            return lastWriteEvent.lock()->getExpectedTimeFire();
        }
    } else return this->readyTimeWrite;
}

double Processor::getExpectedOrActualReadyTimeRead() {
    if (!this->lastReadEvent.expired()) {
        if (this->lastReadEvent.lock()->isDone) {
            return this->lastReadEvent.lock()->getActualTimeFire();
        } else {
            return lastReadEvent.lock()->getExpectedTimeFire();
        }
    } else return this->readyTimeRead;
}