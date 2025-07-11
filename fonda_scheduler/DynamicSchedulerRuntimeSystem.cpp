#include <queue>
#include <random>

#include "fonda_scheduler/DynamicSchedulerHeader.hpp"
#include "fonda_scheduler/SchedulerHeader.hpp"
#include "fonda_scheduler/algorithms.hpp"

Cluster* cluster;
EventManager events;
ReadyQueue readyQueue;
int devationVariant;
bool usePreemptiveWrites;

std::string lastEventName;

double dynMedih(graph_t* graph, Cluster* cluster1, const int algoNum, const int deviationNumber, const bool upw)
{
    double resMakespan = -1;
    cluster = cluster1;
    enforce_single_source_and_target_with_minimal_weights(graph);
    compute_bottom_and_top_levels(graph);
    devationVariant = deviationNumber;
    usePreemptiveWrites = upw;

    static std::mt19937 gen(std::random_device {}());
    static std::uniform_real_distribution<double> dist(0.0, 1);

    switch (algoNum) {
    case fonda_scheduler::HEFT:
    case fonda_scheduler::HEFT_BL: {
        vertex_t* vertex = graph->first_vertex;

        while (vertex != nullptr) {
            double rank = calculateSimpleBottomUpRank(vertex);
            rank = rank + dist(gen);
            vertex->rank = rank;
            vertex = vertex->next;
        }
        break;
    }
    case fonda_scheduler::HEFT_BLC: {
        vertex_t* vertex = graph->first_vertex;

        while (vertex != nullptr) {
            double rank = calculateBLCBottomUpRank(vertex);
            rank = rank + dist(gen);
            vertex->rank = rank;
            vertex = vertex->next;
        }
        break;
    }
    case fonda_scheduler::HEFT_MM: {
        std::vector<std::pair<vertex_t*, double>> ranks = calculateMMBottomUpRank(graph);
        std::for_each(ranks.begin(), ranks.end(), [](const std::pair<vertex_t*, double>& pair) {
            pair.first->rank = pair.second + dist(gen);
        });
        break;
    }
    default:
        throw std::runtime_error("unknown algorithm");
    }

    if (findVertexByName(graph, "GRAPH_SOURCE") != nullptr) {
        remove_vertex(graph, findVertexByName(graph, "GRAPH_SOURCE"));
        remove_vertex(graph, findVertexByName(graph, "GRAPH_TARGET"));
    }

    vertex_t* vertex = graph->first_vertex;
    while (vertex != nullptr) {
        if (vertex->in_edges.empty()) {
            //  cout << "starting task " << vertex->name << endl;
            std::vector<std::shared_ptr<Processor>> bestModifiedProcs;
            std::shared_ptr<Processor> bestProcessorToAssign;
            std::vector<std::shared_ptr<Event>> newEvents = bestTentativeAssignment(vertex, bestModifiedProcs, bestProcessorToAssign, 0);

            for (auto& item : newEvents) {
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
        std::shared_ptr<Event> firstEvent = events.getEarliest();
        bool removed = events.remove(firstEvent->id);
        assert(removed == true);

        if (firstEvent->id == lastEventName && !firstEvent->getPredecessors().empty()) {
            // cout << "FIRE SAME EVENT " << lastEventName << " with predecessor "
            //    << (*firstEvent->getPredecessors().begin())->id << endl;
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
            events.insert(firstEvent);
            while (!firstEvent->getPredecessors().empty() && !(*firstEvent->getPredecessors().begin())->isDone) {
                firstEvent = *firstEvent->getPredecessors().begin();
            }
            // cout << "2-firing predecessor without predecessors " << firstEvent->id <<" done? "<<(firstEvent->isDone?"yes":"no") << endl;
            removed = events.remove(firstEvent->id);
            // assert(removed == true);
        }

        if (removed || !firstEvent->isDone) {
            //  cout<<"finally event "<<firstEvent->id<<endl;
            firstEvent->fire();
            resMakespan = std::max(resMakespan, firstEvent->getActualTimeFire());
            lastEventName = firstEvent->id;
            auto ptr = events.findByEventId(firstEvent->id);
        } else {
            std::cout << "nthng " << '\n';
            return -1;
        }
        //        assert(ptr == nullptr);

        //  cout<<"events now "; events.printAll();
    }

    return resMakespan;
}

void Event::fireTaskStart()
{
    std::string thisid = this->id;
    // cout << " firing task start for " << thisid << " at " << this->actualTimeFire << " on proc " << this->processor->id << endl;

    //   cout<<this->task->name<<" "<<this->actualTimeFire<<" "<<events.find(this->task->name+"-f")->getActualTimeFire()<<" on "<<this->processor->id<<endl;

    const auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        // cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
        //      << endl;
        // this->setActualTimeFire(
        //       this->getActualTimeFire()  +std::numeric_limits<double>::epsilon() * this->getActualTimeFire());
        events.insert(shared_from_this());
        return;
    }

    // cout << "DONE" << endl;
    removeOurselfFromSuccessors(this);
    this->task->status = Status::Running;
    const auto ourFinishEvent = events.findByEventId(this->task->name + "-f");
    if (ourFinishEvent == nullptr) {
        throw std::runtime_error("NOt found finish event to " + this->task->name);
    }
    double durationTask = ourFinishEvent->getExpectedTimeFire() - this->getExpectedTimeFire();
    assert(durationTask > 0);
    assert(this->task->name == "GRAPH_SOURCE" || durationTask >= this->task->time / this->processor->getProcessorSpeed()
        || abs(durationTask - this->task->time / this->processor->getProcessorSpeed()) < 0.1);

    const auto factor = applyDeviationTo(durationTask);
    this->task->factorForRealExecution = factor;
    assert(factor > 0);
    for (auto successor = successors.begin(); successor != successors.end();) {
        // cout << " from " << (*successor)->id << "'s predecessors" << endl;
        if ((*successor)->task == nullptr) {
            throw std::runtime_error("edge-based event depends on task start " + this->id);
        }
        ++successor;
    }

    const double d = this->getActualTimeFire() + durationTask;
    // cout << "on start  setting finish time from "<< ourFinishEvent->actualTimeFire <<" to " << d << endl;
    // ourFinishEvent->setActualTimeFire(d);
    events.update(ourFinishEvent->id, d);

    for (auto inEdge : this->task->in_edges) {
        const std::string& edgeName = buildEdgeName(inEdge);
        std::shared_ptr<Event> startWrite = events.findByEventId(edgeName + "-w-s");
        std::shared_ptr<Event> finishWrite = events.findByEventId(buildEdgeName(inEdge) + "-w-f");
        if (startWrite != nullptr) {
            events.remove(startWrite->id);
            events.remove(finishWrite->id);
            removeOurselfFromSuccessors(startWrite.get());
            removeOurselfFromSuccessors(finishWrite.get());
            startWrite->isDone = true;
            finishWrite->isDone = true;

            std::unordered_set<Event*> visited;
            propagateChainInPlanning(finishWrite,
                startWrite->getActualTimeFire() - finishWrite->getActualTimeFire(), visited);
        } else {
            if (finishWrite != nullptr) {
                finishWrite->setActualTimeFire(this->getActualTimeFire());
            }
        }

        for (auto& [proc_id, processor] : cluster->getProcessors()) {
            if (processor->writingQueue.empty()) {
                continue;
            }

            // Erase the inEdge from the writingQueue of the processor (if it exists)
            processor->writingQueue.erase(
                std::remove(processor->writingQueue.begin(), processor->writingQueue.end(), inEdge),
                processor->writingQueue.end());
        }
    }
    // cout << endl;
}

void Event::fireTaskFinish()
{
    const vertex_t* thisTask = this->task;
    //  cout << "firing task Finish for " << this->id << " at " << this->getActualTimeFire() << endl;
    //  cout<<this->actualTimeFire<<" on "<<this->processor->id<<endl;

    const auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        //     cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
        //       << endl;
        events.insert(shared_from_this());
        return;
    }

    // cout << "DONE " << endl;
    removeOurselfFromSuccessors(this);
    // set its status to finished
    this->task->status = Status::Finished;
    this->isDone = true;
    this->task->makespan = this->actualTimeFire;

    assert(cluster->getProcessorById(this->processor->id).use_count() == this->processor.use_count());

    assert(this->processor->getAvailableMemory() <= this->processor->getMemorySize());

    std::string thisId = this->id;

    for (const auto out_edge : thisTask->out_edges) {
        locateToThisProcessorFromNowhere(out_edge, this->processor->id, false,
            this->getActualTimeFire());
    }

    for (auto out_edge : thisTask->out_edges) {
        vertex_t* childTask = out_edge->head;
        // cout << "deal with child " << childTask->name << endl;
        bool isReady = true;
        for (const auto & in_edge : childTask->in_edges) {
            if (in_edge->tail->status == Status::Unscheduled) {
                isReady = false;
            }
        }

        std::vector<std::shared_ptr<Event>> pred, succ;

        if (isReady && childTask->status != Status::Scheduled) {
            // cout<<"inserting into ready "<<childTask->name<<endl;
            readyQueue.readyTasks.insert(childTask);
        }

        if (usePreemptiveWrites) {
            cluster->getProcessorById(this->processor->id)->writingQueue.emplace_back(out_edge);
        }
    }

    this->isDone = true;

    bool foundSomeTaskForOurProcessor = false;
    bool existsIdleProcessor = false;

    while ((!foundSomeTaskForOurProcessor || existsIdleProcessor) && !readyQueue.readyTasks.empty()) {
        vertex_t* mostReadyVertex = *readyQueue.readyTasks.begin();
        std::vector<std::shared_ptr<Processor>> bestModifiedProcs;
        std::shared_ptr<Processor> bestProcessorToAssign;
        // cout<<"assigning vertex "<<mostReadyVertex->name<<" ";
        std::vector<std::shared_ptr<Event>> newEvents = bestTentativeAssignment(mostReadyVertex, bestModifiedProcs, bestProcessorToAssign,
            this->actualTimeFire);

        mostReadyVertex->status = Status::Scheduled;
        readyQueue.readyTasks.erase(mostReadyVertex);

        if (bestProcessorToAssign->id == this->processor->id) {
            foundSomeTaskForOurProcessor = true;
        }

        for (auto& item : bestModifiedProcs) {
            //  cout<<item.use_count()<<" ";
            item.reset();
        }
        //  cout<<endl;
        // cout<<bestProcessorToAssign.use_count()<<endl;
        bestProcessorToAssign.reset();

        for (const auto& item : newEvents) {
            assert(cluster->getProcessorById(item->processor->id).use_count() == item->processor.use_count());
            events.insert(item);
            // cout<<"new event "<<item->id<<" w preds "<<endl;
            // for (const auto &item1: item->predecessors){
            //    cout<<item1->id<<", ";
            //}
            // cout <<endl<<"successors ";
            // for (const auto &item1: item->successors){
            //     cout<<item1->id<<", ";
            // }
            //  cout <<endl;
        }

        existsIdleProcessor = false;
        for (const auto& [proc_id, processor] : cluster->getProcessors()) {
            if (processor->getReadyTimeCompute() < this->getActualTimeFire()) {
                existsIdleProcessor = true;
                break;
            }
        }
    }
}

std::shared_ptr<Processor> findPredecessorsProcessor(const edge_t* incomingEdge, std::vector<std::shared_ptr<Processor>>& modifiedProcs)
{
    const vertex_t* predecessor = incomingEdge->tail;
    auto predecessorsProcessorsId = predecessor->assignedProcessorId;
    // assert(isLocatedOnThisProcessor(incomingEdge, predecessorsProcessorsId));
    std::shared_ptr<Processor> addedProc;
    const auto it = // modifiedProcs.size()==1?
                    //   modifiedProcs.begin():
        std::find_if(modifiedProcs.begin(), modifiedProcs.end(),
            [predecessorsProcessorsId](const std::shared_ptr<Processor>& p) {
                return p->id == predecessorsProcessorsId;
            });

    if (it == modifiedProcs.end()) {
        addedProc = std::make_shared<Processor>(*cluster->getProcessorById(predecessorsProcessorsId));
        // cout << "adding modified proc " << addedProc->id << endl;
        modifiedProcs.emplace_back(addedProc);
        // checkIfPendingMemoryCorrect(addedProc);
    } else {
        addedProc = *it;
    }

    return addedProc;
}

void Event::fireReadStart()
{
    // cout << "firing read start for " << this->id << " at " << this->actualTimeFire << endl;
    // assert(finishRead->getActualTimeFire()> this->getActualTimeFire());
    const auto canRun = dealWithPredecessors(shared_from_this());

    if (!canRun) {
        //  cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
        //      << endl;
        events.insert(shared_from_this());
        return;
    }

    // cout << "DONE" << endl;
    removeOurselfFromSuccessors(this);
    double durationOfRead = this->edge->weight / this->processor->readSpeedDisk;
    const double factor = applyDeviationTo(durationOfRead);
    assert(factor > 0);
    this->edge->factorForRealExecution = factor;
    const double expectedTimeFireFinish = this->actualTimeFire + durationOfRead;

    this->isDone = true;

    const std::shared_ptr<Event> finishRead = events.findByEventId(buildEdgeName(this->edge) + "-r-f");
    if (finishRead == nullptr) {
        throw std::runtime_error("NO read finish found for " + this->id);
    }

    events.update(buildEdgeName(this->edge) + "-w-f", expectedTimeFireFinish);

    //  cout << endl;
}

void Event::fireReadFinish()
{
    //  cout << "firing read finish for " << this->id << " at " << this->getActualTimeFire() << " on "
    //      << this->processor->id << endl;

    std::shared_ptr<Event> startRead = events.findByEventId(buildEdgeName(this->edge) + "-r-s");

    const auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        //  cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
        //    << endl;
        events.insert(shared_from_this());
    } else {
        // cout << "DONE " << endl;
        removeOurselfFromSuccessors(this);
        assert(cluster->getProcessorById(this->processor->id).use_count() == this->processor.use_count());
        if (!isLocatedOnDisk(this->edge, false)) {
            const auto ptr = events.findByEventId(buildEdgeName(this->edge) + "-w-f");
            assert(ptr != nullptr);
            auto ptr1 = events.findByEventId(buildEdgeName(this->edge) + "-r-s");
            assert(ptr->getActualTimeFire() < this->getActualTimeFire());
        }
        locateToThisProcessorFromDisk(this->edge, this->processor->id, false, this->getActualTimeFire());
        this->isDone = true;
    }
}

void Event::fireWriteStart()
{
    // cout << "firing write start for " << this->id << " at " << this->getActualTimeFire() << endl;

    const auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        //   cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
        //        << endl;
        events.insert(shared_from_this());
        return;
    }

    // cout << "DONE" << endl;
    removeOurselfFromSuccessors(this);

    assert(cluster->getProcessorById(this->processor->id).use_count() == this->processor.use_count());

    double durationOfWrite = this->edge->weight / this->processor->writeSpeedDisk;
    const double factor = applyDeviationTo(durationOfWrite);
    this->edge->factorForRealExecution = factor;
    assert(factor > 0);

    const double actualTimeFireFinish = this->actualTimeFire + durationOfWrite;
    this->isDone = true;
    const std::shared_ptr<Event> finishWrite = events.findByEventId(buildEdgeName(this->edge) + "-w-f");

    if (finishWrite == nullptr) {
        throw std::runtime_error("NO write finish found for " + this->id);
    }

    events.update(buildEdgeName(this->edge) + "-w-f", actualTimeFireFinish);
    assert(!finishWrite->checkCycleFromEvent());

    std::string thisid = buildEdgeName(this->edge);
    const auto edgeInWritingQueue = std::find(this->processor->writingQueue.begin(), this->processor->writingQueue.end(),
        this->edge);
    if (edgeInWritingQueue != this->processor->writingQueue.end()) {
        // cluster->getProcessorById(this->processor->id)->writingQueue.erase(edgeInWritingQueue);
        this->processor->writingQueue.erase(edgeInWritingQueue);
    }
}

void Event::fireWriteFinish()
{
    //  cout << "firing write finish for " << this->id << " at " << this->getActualTimeFire() << " on "
    //      << this->processor->id << endl;

    const auto canRun = dealWithPredecessors(shared_from_this());
    if (!canRun) {
        // cout << "BAD because #preds " << this->predecessors.size() << " esp " << (*this->predecessors.begin())->id
        //     << endl;
        events.insert(shared_from_this());
        return;
    }

    // cout << "DONE" << endl;
    removeOurselfFromSuccessors(this);
    if (this->onlyPreemptive) {
        locateToDisk(this->edge, false, this->getActualTimeFire());
        assert(isLocatedOnThisProcessor(this->edge, this->processor->id, false));
    } else {
        delocateFromThisProcessorToDisk(this->edge, this->processor->id, false, this->getActualTimeFire());
    }
    assert(cluster->getProcessorById(this->processor->id).use_count() == this->processor.use_count());

    const auto positionInWriteQ = std::find(this->processor->writingQueue.begin(), this->processor->writingQueue.end(),
        this->edge);
    if (positionInWriteQ != this->processor->writingQueue.end()) {
        this->processor->writingQueue.erase(positionInWriteQ);
    }

    this->isDone = true;

    if (this->processor->writingQueue.empty()) {
        return;
    }

    edge_t* edgeToWriteJustInCase = this->processor->writingQueue.at(0);

    if (events.findByEventId(buildEdgeName(edgeToWriteJustInCase) + "-w-s") != nullptr || events.findByEventId(buildEdgeName(edgeToWriteJustInCase) + "-w-f") != nullptr) {
        // cout << "event for " << buildEdgeName(edgeToWriteJustInCase) << " already in queue" << endl;
        this->processor->writingQueue.erase(this->processor->writingQueue.begin());
        return;
    }

    const double presumedLength = assessWritingOfEdge(edgeToWriteJustInCase, this->processor);
    double startOfNextWrite = std::numeric_limits<double>::max();
    const std::set<std::shared_ptr<Event>, CompareByTimestamp> eventsOnThisProc = events.findByProcessorId(
        this->processor->id);
    assert((*eventsOnThisProc.begin())->getActualTimeFire() <= (*eventsOnThisProc.rbegin())->getActualTimeFire());

    for (auto& item : eventsOnThisProc) {
        //  cout<<"event on proc "<<item->id<<" at "<<item->getActualTimeFire()<<endl;
        // events are ordered by timestamp, so iterating from earliest to latest
        if (item->type == eventType::OnWriteStart && item->getActualTimeFire() > this->getActualTimeFire()) {
            startOfNextWrite = item->getActualTimeFire();
            break;
        }
    }

    if (this->getActualTimeFire() + presumedLength < startOfNextWrite) {
        // can fit
        // cout << "scheduling extra write for " << buildEdgeName(edgeToWriteJustInCase) << endl;
        assert(events.findByEventId(buildEdgeName(edgeToWriteJustInCase) + "-w-s") == nullptr);
        assert(events.findByEventId(buildEdgeName(edgeToWriteJustInCase) + "-w-f") == nullptr);
        std::pair<std::shared_ptr<Event>, std::shared_ptr<Event>> writeEvents;
        scheduleWriteForEdge(this->processor, edgeToWriteJustInCase, writeEvents, true);
        events.insert(writeEvents.first);
        events.insert(writeEvents.second);
        assert(isLocatedOnThisProcessor(edgeToWriteJustInCase, this->processor->id, false));
        this->processor->writingQueue.erase(this->processor->writingQueue.begin());
        if (buildEdgeName(edgeToWriteJustInCase) == "preseq_00000155-multiqc_00000149") {
            std::cout << '\n';
        }
        // for (const auto &item: this->processor->writingQueue){
        //    cout<<buildEdgeName(item)<<endl;
        // }
        // cout<<"----------"<<endl;
        // for (const auto &item: cluster->getProcessorById(this->processor->id)->writingQueue){
        //     cout<<buildEdgeName(item)<<endl;
        //  }
    }
}

void Event::removeOurselfFromSuccessors(const Event* us)
{
    // cout << "removing from successors us, " << us->id;

    for (auto successor = successors.begin(); successor != successors.end();) {
        // cout << " from " << (*successor)->id << "'s predecessors "; //<< endl;
        for (auto succspred = (*successor)->predecessors.begin(); succspred != (*successor)->predecessors.end();) {
            if ((*succspred)->id == us->id) {
                // cout << "removed" << endl;
                succspred = (*successor)->predecessors.erase(succspred);
            } else {
                // Move to the next element
                ++succspred;
            }
        }
        // if (!isREmoved) cout << "NOT REMOVED FROM SUCCESSOR " << (*successor)->id << endl;
        ++successor;
    }
    // successors.clear();
    /*cout << "sanity check!" << endl;
    for (const auto &s: successors) {
        std::cout << "Successor " << s->id << " has predecessors: ";
        for (const auto &pred: s->predecessors) {
            std::cout << pred->id << " ";
        }
        std::cout << '\n';
    } */
}

void Cluster::printProcessorsEvents()
{

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

void Processor::addEvent(const std::shared_ptr<Event>& event)
{
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

bool dealWithPredecessors(const std::shared_ptr<Event>& us)
{
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
        for (const auto& item : us->getPredecessors()) {
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

void transferAfterMemoriesToBefore(const std::shared_ptr<Processor>& ourModifiedProc)
{
    ourModifiedProc->resetPendingMemories();
    ourModifiedProc->setAvailableMemory(ourModifiedProc->getMemorySize());
    for (auto& item : ourModifiedProc->getAfterPendingMemories()) {
        ourModifiedProc->addPendingMemory(item);
    }
    ourModifiedProc->setAvailableMemory(ourModifiedProc->getAfterAvailableMemory());
    ourModifiedProc->resetAfterPendingMemories();
    ourModifiedProc->setAfterAvailableMemory(ourModifiedProc->getMemorySize());
}

double applyDeviationTo(double& in)
{
    static std::random_device rd;
    static std::mt19937 gen(rd()); // Mersenne Twister PRNG

    double stddev;
    switch (devationVariant) {
    case 1:
        stddev = in * 0.1;
        break;
    case 2:
        stddev = in * 0.5;
        break;
    case 3:
    case 4:
        stddev = 0;
        break;
    case 5:
        stddev = in * 0.3;
        break;
    default:
        throw std::runtime_error("unknown deviation variant");
    }

    std::normal_distribution<double> dist(in, stddev);
    double result = dist(gen);
    result = std::max(result, 1.0);
    if (devationVariant == 4) {
        result *= 2;
    }
    const double factor = result / in;
    in = result;
    return factor;
}

void Processor::setLastWriteEvent(const std::shared_ptr<Event>& lwe)
{
    this->lastWriteEvent = lwe;
    this->readyTimeWrite = lwe->getActualTimeFire();
}

void Processor::setLastReadEvent(const std::shared_ptr<Event>& lre)
{
    this->lastReadEvent = lre;
    this->readyTimeRead = lre->getActualTimeFire();
}

void Processor::setLastComputeEvent(const std::shared_ptr<Event>& lce)
{
    this->lastComputeEvent = lce;
    this->readyTimeCompute = lce->getActualTimeFire();
}

double Processor::getReadyTimeCompute()
{
    if (!this->lastComputeEvent.expired() && this->lastComputeEvent.lock()->getActualTimeFire() != this->readyTimeCompute) {
        this->readyTimeCompute = lastComputeEvent.lock()->getActualTimeFire();
    }
    return this->readyTimeCompute;
}

double Processor::getReadyTimeWrite()
{
    if (!this->lastWriteEvent.expired() && this->lastWriteEvent.lock()->getActualTimeFire() != this->readyTimeWrite) {
        this->readyTimeWrite = lastWriteEvent.lock()->getActualTimeFire();
    }
    return this->readyTimeWrite;
}

double Processor::getReadyTimeRead()
{
    if (!this->lastReadEvent.expired() && this->lastReadEvent.lock()->getActualTimeFire() != this->readyTimeRead) {
        this->readyTimeRead = lastReadEvent.lock()->getActualTimeFire();
    }
    return this->readyTimeRead;
}

double Processor::getExpectedOrActualReadyTimeCompute() const
{
    if (const auto lastEvent = this->lastComputeEvent.lock()) {
        return lastEvent->isDone ? lastEvent->getActualTimeFire() : lastEvent->getExpectedTimeFire();
    }
    return this->readyTimeCompute;
}

double Processor::getExpectedOrActualReadyTimeWrite() const
{
    if (const auto lastEvent = this->lastWriteEvent.lock()) {
        return lastEvent->isDone ? lastEvent->getActualTimeFire() : lastEvent->getExpectedTimeFire();
    }
    return this->readyTimeWrite;
}

double Processor::getExpectedOrActualReadyTimeRead() const
{
    if (const auto& lastEvent = this->lastReadEvent.lock()) {
        return lastEvent->isDone ? lastEvent->getActualTimeFire() : lastEvent->getExpectedTimeFire();
    }
    return this->readyTimeRead;
}