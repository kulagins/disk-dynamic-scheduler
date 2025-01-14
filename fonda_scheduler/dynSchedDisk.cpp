
#include <queue>
#include "fonda_scheduler/dynSchedDisk.hpp"
#include "fonda_scheduler/dynSched.hpp"

Cluster* cluster;
EventManager events;

double new_heuristic_dynamic(graph_t *graph, Cluster *cluster, int algoNum, bool isHeft){
    algoNum = isHeft? 1: algoNum;
    vector<pair<vertex_t *, double>> ranks = calculateBottomLevels(graph, algoNum);
    removeSourceAndTarget(graph, ranks);
    sort(ranks.begin(), ranks.end(),
         [](pair<vertex_t *, double> a, pair<vertex_t *, double> b) {
             return a.second > b.second;

         });

    vertex_t *vertex = graph->first_vertex;
    while (vertex != nullptr) {
        if(vertex->in_degree==0){
            std::vector<Event> pred, succ;
            events.insert(Event(vertex, nullptr, eventType::OnTaskStart, cluster->getMemBiggestFreeProcessor(), 0.0,0.0, pred,succ,false, vertex->name+"-s"));
        }
        vertex = vertex->next;
    }
    int cntr=0;
    while(!events.empty()){
        cout<<"NEXT "; //events.printAll();
        cntr++;
        Event *firstEvent = events.getEarliest();
        cout<<"event "<<firstEvent->id<<" at "<<firstEvent->actualTimeFire<<endl;
        firstEvent->fire(cluster, events);
        bool removed = events.remove(firstEvent->id);
        assert(removed==true);
        //cout<<"events now "; events.printAll();
    }
  //  cout<<cntr<<" "<<graph->number_of_vertices<<endl;
    cout<<cluster->getMemBiggestFreeProcessor()->readyTimeCompute<<endl;

    return -1;
}

void  Event::fireTaskStart(Cluster * cluster, EventManager &eventsP){
    cout<<"firing task start for "<<this->id<<endl;
    double timeStart= 0;
    if(!this->predecessors.empty()){
        for (const auto &item: predecessors){
            if(item.actualTimeFire>timeStart){
                timeStart= item.actualTimeFire;
            }
        }
        this->actualTimeFire= timeStart;
        eventsP.insert(*this);
    }
    else{
        std::vector<Event> pred, succ;
        //TODO maybe est ft - est st
        double actualRuntime = deviation(this->task->time/this->processor->getProcessorSpeed());
        double d = this->actualTimeFire + actualRuntime;
        eventsP.insert(Event(this->task, nullptr, eventType::OnTaskFinish, cluster->getMemBiggestFreeProcessor(), d,
                       d, pred, succ, false, this->task->name+"-f"));
        this->processor->setReadyTimeCompute(d);
    }
}
void  Event::fireTaskFinish(Cluster * cluster,EventManager &eventsP){
    vertex_t *thisTask = this->task;
    cout << "firing task Finish for " << this->id << endl;

    // free its memory
    this->processor->availableMemory+= task->actuallyUsedMemory;
    //set its status to finished
    thisTask->status= Status::Finished;
    string thisId= this->id;

    for ( Event successor: this->successors){
        //deletes itself from successors' predecessors list
        successor.predecessors.erase(find_if(successor.predecessors.begin(), successor.predecessors.end(), [thisId](Event e){
            return thisId== e.id; // thisTask->name==e.task->name;
        }));
        // updates successors' fire time
        successor.actualTimeFire= this->actualTimeFire;

    }

    //Then task goes over its successor tasks in the workflow and schedules ready ones.
    for(int i=0; i < thisTask->out_degree; i++){
        std::vector<Event> pred, succ;
        vertex_t *childTask = thisTask->out_edges[i]->head;
        cout<<"deal with child "<<childTask->name<<endl;
        bool isReady=true;
        for(int j=0; j < thisTask->in_degree; j++){
              if(childTask->in_edges[j]->tail->status!= Status::Finished){
                  isReady=false;
              }
        }

        //TODO Can be two times in queue?
        if(true) {//isReady
            eventsP.insert(Event(childTask, nullptr, eventType::OnTaskStart,
                           cluster->getMemBiggestFreeProcessor(), this->actualTimeFire, this->actualTimeFire, pred, succ, false, childTask->name+"-s"));
            childTask->status= Status::Ready;
        }
    }
}
void  Event::fireReadStart(Cluster * clusterP, EventManager& eventsP){
    cout<<"firing read start for "; print_edge(this->edge);
}
void  Event::fireReadFinish(Cluster * clusterP, EventManager &eventsP){
    cout<<"firing read finish for "; print_edge(this->edge);
}
void  Event::fireWriteStart(Cluster * clusterP, EventManager& eventsP){
    cout<<"firing write start for "; print_edge(this->edge);
}
void  Event::fireWriteFinish(Cluster * clusterP, EventManager& eventsP){
    cout<<"firing write finish for "; print_edge(this->edge);
}
vector<vertex_t*> getReadyTasks(graph_t *graph){
    vector<vertex_t*> res;
    vertex_t *vertex = graph->first_vertex;
    while (vertex != nullptr) {
        bool isReady=true;
        for(int i=0; i<vertex->in_degree; i++){
            if(vertex->in_edges[i]->tail->status!= Status::Finished){
                isReady=false;
                break;
            }
        }
        if(isReady){
            res.emplace_back(vertex);
        }
        vertex = vertex->next;
    }
    return res;
}

void onTaskFinish(Event event){

    assert(event.type==eventType::OnTaskFinish);
    assert(event.task!=NULL);

    //free memory - not being done

    //incoming edges are nowhere
    for(int j=0; j<event.task->in_degree; j++){
        event.task->in_edges[j]->locations= { Location(LocationType::Nowhere)};
    }
    //set event to ready
    event.task->status = Status::Finished;

    for (auto &successor: event.successors){
        successor.predecessors.erase(
                std::remove_if(
                        successor.predecessors.begin(),
                        successor.predecessors.end(),
                        [event](const Event& event1) {
                            return event1.task->name == event.task->name && event1.expectedTimeFire == event.expectedTimeFire;
                        }
                ),
                successor.predecessors.end()
        );
    }



}

vector<Event> tryScheduleTask(vertex_t* task){

}



void
processIncomingEdges(Event &startComputingVertex, shared_ptr<Processor> &ourDesiredProc, double &earliestStartingTimeToComputeVertex,
                     double timeNow) {
    earliestStartingTimeToComputeVertex = ourDesiredProc->readyTimeCompute;
    vertex_t* v = startComputingVertex.task;
    for (int j = 0; j < v->in_degree; j++) {
        edge *incomingEdge = v->in_edges[j];
        vertex_t *predecessor = incomingEdge->tail;
        string eventId = predecessor->name+"->"+v->name;
        if(!isLocatedOnThisProcessor(incomingEdge, ourDesiredProc->id)){
            if(isLocatedOnDisk(incomingEdge)){
                Event *endWriteOfSameFile = events.find(eventId + "-w-f");
                if(endWriteOfSameFile == nullptr){
                    throw runtime_error("No write finish found");
                }

                double estimatedTimeOfRead= incomingEdge->weight / ourDesiredProc->readSpeedDisk;
                double estimatedReadStart = earliestStartingTimeToComputeVertex - estimatedTimeOfRead;
                estimatedReadStart= max(estimatedReadStart, endWriteOfSameFile->actualTimeFire);
                //TODO IMPORTANT!! CHECK IF THE READ STARS DURING ANOTHER TASK AND RESCHEDULE IF NOT ENOUGH MEM!
                std::vector<Event> pred, succ;
                pred.emplace_back(*endWriteOfSameFile);
                succ.emplace_back(startComputingVertex);
                Event readFromDisk = Event(nullptr, incomingEdge, eventType::OnReadStart, ourDesiredProc,  estimatedReadStart,
                                           estimatedReadStart, pred, succ,false, eventId+"s");
                ourDesiredProc->readyTimeRead +=
                earliestStartingTimeToComputeVertex = ourDesiredProc->readyTimeRead > earliestStartingTimeToComputeVertex ?
                                                      ourDesiredProc->readyTimeRead : earliestStartingTimeToComputeVertex;
            }
            else if(isLocatedNowhere(incomingEdge)){
                throw runtime_error("Located nowhere "+eventId);
            }
            else{
                assert(isLocatedOnAnyProcessor(incomingEdge));

                //ASSUMING IT IS LOCATED ON THE PROC OF PREDECESSOR!!
                assert(isLocatedOnThisProcessor(incomingEdge, predecessor->assignedProcessorId));
              //  Event writeToDisk = Event(nullptr, incomingEdge, eventType::OnWriteStart, ourDesiredProc,  estimatedReadStart,
               // TODO                            estimatedReadStart, pred, succ,false, eventId+"s");

            }

        }
        /*

            if (isLocatedOnDisk(incomingEdge)) {
                //we need to schedule read
                ourDesiredProc->readyTimeRead += incomingEdge->weight / ourDesiredProc->readSpeedDisk;
                earliestStartingTimeToComputeVertex = ourDesiredProc->readyTimeRead > earliestStartingTimeToComputeVertex ?
                                                      ourDesiredProc->readyTimeRead : earliestStartingTimeToComputeVertex;
                //TODO evict??
            } else {
                auto predecessorsProcessorsId = predecessor->assignedProcessorId;
                assert(isLocatedOnThisProcessor(incomingEdge, predecessorsProcessorsId));
                shared_ptr<Processor>  addedProc;
                auto it = //modifiedProcs.size()==1?
                        //  modifiedProcs.begin():
                        std::find_if(modifiedProcs.begin(), modifiedProcs.end(),
                                     [predecessorsProcessorsId](const shared_ptr<Processor>& p) {
                                         return p->id == predecessorsProcessorsId;
                                     });

                if(it==modifiedProcs.end()){
                    addedProc = make_shared<Processor>(*cluster->getProcessorById(predecessorsProcessorsId));
                    // cout<<"adding modified proc "<<addedProc->id<<endl;
                    modifiedProcs.emplace_back(addedProc);
                    checkIfPendingMemoryCorrect(addedProc);
                }
                else{
                    addedProc = *it;
                }

                assert(!hasDuplicates(modifiedProcs));

                double timeToStartWriting= max(predecessor->makespan, addedProc->readyTimeWrite);
                addedProc->readyTimeWrite= timeToStartWriting+ incomingEdge->weight / addedProc->writeSpeedDisk;
                double startTimeOfRead = max(addedProc->readyTimeWrite, ourDesiredProc->readyTimeRead);
                ourDesiredProc->readyTimeRead = startTimeOfRead + incomingEdge->weight / ourDesiredProc->readSpeedDisk;
                earliestStartingTimeToComputeVertex = ourDesiredProc->readyTimeRead > earliestStartingTimeToComputeVertex ?
                                                      ourDesiredProc->readyTimeRead : earliestStartingTimeToComputeVertex;
                //int addpl  = addedProc->pendingMemories.size();
                addedProc->removePendingMemory(incomingEdge);
                // assert(addpl> addedProc->pendingMemories.size());
                checkIfPendingMemoryCorrect(addedProc);
            }
            */
        }


}


double deviation(double in){
    return in; //in* 2;
}