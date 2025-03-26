//
// Created by kulagins on 26.05.23.
//

#include "../include/fonda_scheduler/common.hpp"

#include "graph.hpp"
#include "fonda_scheduler/dynSched.hpp"

bool Debug;

void delayEverythingBy(vector<Assignment*> &assignments, Assignment * startingPoint, double delayTime){

    for (auto &assignment: assignments){
        if(assignment->startTime>= startingPoint->startTime){
            assignment->startTime+=delayTime;
            assignment->finishTime+=delayTime;
        }
    }
}


std::string trimQuotes(const std::string& str) {
    if (str.empty()) {
        return str; // Return the empty string if input is empty
    }

    string result = str, prevResult;
   do{
        prevResult = result;
        size_t start = 0;
        size_t end = prevResult.length() - 1;

        // Check for leading quote
        if (prevResult[start] == '"'||prevResult[start] == '\\'|| prevResult[start]==' ') {
            start++;
        }

        // Check for trailing quote
        if (prevResult[end] == '"'|| prevResult[end]=='\\'|| prevResult[end]==' ') {
            end--;
        }

        result = prevResult.substr(start, end - start + 1);
    } while(result!=prevResult);
    return result;
}



void Cluster::printAssignment(){
    int counter =0;
    for (const auto &item: this->getProcessors()){
        if(item.second->isBusy){
            counter++;
            cout<<"Processor"<<counter<<"."<<endl;
            cout<<"\tMem: "<<item.second->getMemorySize()<<", Proc: "<<item.second->getProcessorSpeed()<<endl;

            vertex_t *assignedVertex = item.second->getAssignedTask();
            if(assignedVertex==NULL)
                cout<<"No assignment."<<endl;
            else{
                cout<<"Assigned subtree, id "<<assignedVertex->id<< ", leader "<<-1<<", memReq "<< assignedVertex->memoryRequirement<<endl;
                for (vertex_t *u = assignedVertex->subgraph->source; u; u = next_vertex_in_topological_order(assignedVertex->subgraph, u)) {
                    cout<<u->name<<", ";
                }
                cout<<endl;
            }
        }
    }
}


void printDebug(string str){
    if(Debug){
        cout<<str<<endl;
    }
}
void printInlineDebug(string str){
    if(Debug){
        cout<<str;
    }
}


void checkForZeroMemories(graph_t *graph) {
    for (vertex_t *vertex = graph->source; vertex; vertex = next_vertex_in_topological_order(graph, vertex)) {
        if (vertex->memoryRequirement == 0) {
            printDebug("Found a vertex with 0 memory requirement, name: " + vertex->name);
        }
    }
}

void removeSourceAndTarget(graph_t *graph, vector<pair<vertex_t *, double>> &ranks) {

    auto iterator = find_if(ranks.begin(), ranks.end(),
                            [](pair<vertex_t *, int> pair1) { return pair1.first->name == "GRAPH_SOURCE"; });
    if(iterator!= ranks.end()){
        ranks.erase(iterator);
    }
    iterator = find_if(ranks.begin(), ranks.end(),
                       [](pair<vertex_t *, int> pair1) { return pair1.first->name == "GRAPH_TARGET"; });
    if(iterator!= ranks.end()){
        ranks.erase(iterator);
    }

    vertex_t *startV = findVertexByName(graph, "GRAPH_SOURCE");
    vertex_t *targetV = findVertexByName(graph, "GRAPH_TARGET");
    if(startV!=NULL)
        remove_vertex(graph, startV);
    if(targetV!=NULL)
        remove_vertex(graph, targetV);

}



bool isLocatedNowhere(edge_t* edge){
    auto it = std::find_if(edge->locations.begin(), edge->locations.end(),
                           [](Location location) {
                               return location.locationType == LocationType::Nowhere;
                           });
    return edge->locations.empty() || it != edge->locations.end();
}

bool isLocatedOnDisk(edge_t* edge){
    return std::find_if(edge->locations.begin(), edge->locations.end(),
                        [](Location location) {
                            return location.locationType== LocationType::OnDisk;
                        })
           != edge->locations.end();
}
bool isLocatedOnThisProcessor(edge_t* edge, int id){
    return std::find_if(edge->locations.begin(), edge->locations.end(),
                        [id](Location location) {
                            return location.locationType== LocationType::OnProcessor && location.processorId==id;
                        })
           != edge->locations.end();
}

bool isLocatedOnAnyProcessor(edge_t* edge){
    return std::find_if(edge->locations.begin(), edge->locations.end(),
                        [](Location location) {
                            return location.locationType== LocationType::OnProcessor;
                        })
           != edge->locations.end();
}

int whatProcessorIsLocatedOn(edge_t* edge){
    auto locationOnProcessor = std::find_if(edge->locations.begin(), edge->locations.end(),
                                            [](Location location) {
                                                return location.locationType == LocationType::OnProcessor;
                                            });
    return (locationOnProcessor != edge->locations.end()) ? locationOnProcessor->processorId.value(): -1;

}
std::string buildEdgeName(edge_t* edge){
    return edge->tail->name+"-"+ edge->head->name;
}

void delocateFromThisProcessorToDisk(edge_t* edge, int id){
    auto locationOnThisProcessor = std::find_if(edge->locations.begin(), edge->locations.end(),
                                                [id](Location location) {
                                                    return location.locationType == LocationType::OnProcessor &&
                                                           location.processorId == id;
                                                });
   //cout<<"delocating "; print_edge(edge);
    //assert(locationOnThisProcessor  != edge->locations.end());
    if(locationOnThisProcessor  == edge->locations.end()){
        edge->locations.erase(locationOnThisProcessor);
        if(!isLocatedOnDisk(edge))
            edge->locations.emplace_back(LocationType::OnDisk);

        throw runtime_error("not located on proc");
    }
    edge->locations.erase(locationOnThisProcessor);
    if(!isLocatedOnDisk(edge))
        edge->locations.emplace_back(LocationType::OnDisk);


}

void locateToThisProcessorFromDisk(edge_t* edge, int id){
  //  cout<<"locating to proc "<<id <<" edge "; print_edge(edge);
    if(!isLocatedOnDisk(edge)){
        cout<<"NOT located on disk yet! Write&Read? "<<buildEdgeName(edge)<<endl;
    }
   // assert(isLocatedOnDisk(edge));
    auto locationOnDisk = std::find_if(edge->locations.begin(), edge->locations.end(),
                                       [](Location location) {
                                           return location.locationType == LocationType::OnDisk;
                                       });
    if(!isLocatedOnThisProcessor(edge, id))
        edge->locations.emplace_back(LocationType::OnProcessor, id);
}

void locateToThisProcessorFromNowhere(edge_t* edge, int id){
  //  cout<<"locating from nowhere to proc "<<id <<" edge "; print_edge(edge);
    if(!isLocatedOnThisProcessor(edge, id))
        edge->locations.emplace_back(LocationType::OnProcessor, id);
}

double getSumOut(vertex_t * v){
    double sumOut=0;
    for (int i = 0; i < v->out_degree; i++) {
        sumOut += v->out_edges[i]->weight;
        //      cout<<sumOut<<" by "<<v->out_edges[i]->weight<<endl;
    }
    return sumOut;
}

double getSumIn(vertex_t * v){
    double sumIn=0;
    for (int i = 0; i < v->in_degree; i++) {
        sumIn += v->in_edges[i]->weight;
        //      cout<<sumOut<<" by "<<v->out_edges[i]->weight<<endl;
    }
    return sumIn;
}

void Event::fire(){
    switch(this->type){
        case eventType::OnTaskStart:
            fireTaskStart();
            break;
        case eventType::OnTaskFinish:
            fireTaskFinish();
            break;
        case eventType::OnReadStart:
            fireReadStart();
            break;
        case eventType::OnReadFinish:
            fireReadFinish();
            break;
        case eventType::OnWriteStart:
            fireWriteStart();
            break;
        case eventType::OnWriteFinish:
            fireWriteFinish();
            break;
    }
    this->timesFired++;
}



void Processor::updateFrom(const Processor& other){

    assert(this->assignedTask== nullptr || this->assignedTask->id == other.assignedTask->id);


    std::unordered_map<std::string, std::weak_ptr<Event>> updatedEvents;
    // Keep valid old events and add new ones from 'other'
    for (const auto& [key, weak_event] : other.eventsOnProc) {
        if (auto shared_event = weak_event.lock()) {  // Ensure the weak_ptr is still valid
            updatedEvents[key] = weak_event;  // Insert or update
            shared_event->processor = shared_from_this();
        }
    }
    // Swap the updated map into place
    eventsOnProc.swap(updatedEvents);

    this->readyTimeCompute= other.readyTimeCompute;
    this->readyTimeRead = other.readyTimeRead;
    this->readyTimeWrite = other.readyTimeWrite;

    assert(other.availableMemory<= other.getMemorySize() || abs(other.availableMemory- other.getMemorySize())<0.01);
    this->availableMemory = other.availableMemory;
    set<edge_t *,  std::function<bool(edge_t*, edge_t*)>> updatedMemories(comparePendingMemories);
    // First, add elements that exist in both and new ones from 'other'
    for (auto* mem : other.pendingMemories) {
        updatedMemories.insert(mem);  // Only inserts new ones, duplicates are ignored
    }
    // Swap the updated set into place
    pendingMemories.swap(updatedMemories);

    assert(other.afterAvailableMemory < other.getMemorySize()|| abs(other.afterAvailableMemory - other.getMemorySize()) <0.01);
    this->afterAvailableMemory = other.afterAvailableMemory;
    updatedMemories.clear();
    // First, add elements that exist in both and new ones from 'other'
    for (auto* mem : other.afterPendingMemories) {
        updatedMemories.insert(mem);  // Only inserts new ones, duplicates are ignored
    }
    // Swap the updated set into place
    afterPendingMemories.swap(updatedMemories);

    this->lastReadEvent= other.lastReadEvent;
    this->lastWriteEvent= other.lastWriteEvent;
    this->lastComputeEvent= other.lastComputeEvent;


}

void clearGraph(graph_t* graphMemTopology){
    vertex_t *vertex = graphMemTopology->first_vertex;
    while (vertex != nullptr) {
        vertex->makespan= vertex->makespanPerceived=-1;
        vertex->visited= false;
        vertex->status= Status::Unscheduled;
        vertex->actuallyUsedMemory=-1;
        vertex->rank=-1;
        vertex = vertex->next;
    }

    edge_t * edge = graphMemTopology->first_edge;
    while(edge!= nullptr){
        edge->locations.clear();
        edge = edge->next;
    }
}