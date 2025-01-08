
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
        cout<<"#events: "<<events.size()<<endl;
        events.front().fire(cluster, events);
       // scheduleReadyTasks();
       // playOutExecution(readyTasks.front());
       events.pop();
    }
    cout<<cntr<<" "<<graph->number_of_vertices<<endl;

    return -1;
}

void playOutExecution(vertex_t* task) {

}

void scheduleReadyTasks() {

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