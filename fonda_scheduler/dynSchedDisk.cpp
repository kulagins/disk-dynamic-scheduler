
#include <queue>
#include "fonda_scheduler/dynSchedDisk.hpp"
#include "fonda_scheduler/dynSched.hpp"


queue<vertex_t*> readyTasks;
Cluster* cluster;
queue<Event> events;

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
            events.emplace(vertex, nullptr, eventType::OnTaskStart, cluster->getMemBiggestFreeProcessor(), 0.0,0.0, pred,succ,false);
        }
        vertex = vertex->next;
    }
    int cntr=0;
    while(!events.empty()){
        cntr++;
        Event &firstEvent = events.front();
        firstEvent.fire(cluster, events);
       events.pop();
    }
    cout<<cntr<<" "<<graph->number_of_vertices<<endl;
    cout<<cluster->getMemBiggestFreeProcessor()->readyTimeCompute<<endl;

    return -1;
}

void  Event::fireTaskStart(Cluster * cluster, queue<Event>& events){
    cout<<"firing task start for "<<this->task->name<<endl;
    std::vector<Event> pred, succ;
    double timeStart= max(this->actualTimeFire, this->processor->readyTimeCompute);
    double nextTime= timeStart+ this->task->time/this->processor->getProcessorSpeed();
    events.emplace(this->task, nullptr, eventType::OnTaskFinish, cluster->getMemBiggestFreeProcessor(), nextTime,nextTime, pred,succ,false);
    this->processor->setReadyTimeCompute(nextTime);
}
void  Event::fireTaskFinish(Cluster * cluster, queue<Event>& events){
    cout<<"firing task Finish for "<<this->task->name <<endl;
    for(int i=0; i< this->task->out_degree;i++){
        std::vector<Event> pred, succ;
        if(this->task->out_edges[i]->head->status!= Status::Ready) {
            events.emplace(this->task->out_edges[i]->head, nullptr, eventType::OnTaskStart,
                           cluster->getMemBiggestFreeProcessor(), this->actualTimeFire, this->actualTimeFire, pred, succ, false);
            this->task->out_edges[i]->head->status= Status::Ready;
        }
    }
}
void  Event::fireReadStart(Cluster * cluster, queue<Event>& events){
    cout<<"firing read start for "; print_edge(this->edge);
}
void  Event::fireReadFinish(Cluster * cluster, queue<Event>& events){
    cout<<"firing read finish for "; print_edge(this->edge);
}
void  Event::fireWriteStart(Cluster * cluster, queue<Event>& events){
    cout<<"firing write start for "; print_edge(this->edge);
}
void  Event::fireWriteFinish(Cluster * cluster, queue<Event>& events){
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