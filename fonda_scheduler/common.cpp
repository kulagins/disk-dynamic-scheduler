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
                cout<<"Assigned subtree, id "<<assignedVertex->id<< ", leader "<<assignedVertex->leader<<", memReq "<< assignedVertex->memoryRequirement<<endl;
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

void delocateFromThisProcessorToDisk(edge_t* edge, int id){
    auto locationOnThisProcessor = std::find_if(edge->locations.begin(), edge->locations.end(),
                                                [id](Location location) {
                                                    return location.locationType == LocationType::OnProcessor &&
                                                           location.processorId == id;
                                                });
   // cout<<"delocating "; print_edge(edge);
    assert(locationOnThisProcessor  != edge->locations.end());
    edge->locations.erase(locationOnThisProcessor);
    if(!isLocatedOnDisk(edge))
        edge->locations.emplace_back(LocationType::OnDisk);


}

void locateToThisProcessorFromDisk(edge_t* edge, int id){
   // cout<<"locating to proc "<<id <<" edge "; print_edge(edge);
    assert(isLocatedOnDisk(edge));
    auto locationOnDisk = std::find_if(edge->locations.begin(), edge->locations.end(),
                                       [](Location location) {
                                           return location.locationType == LocationType::OnDisk;
                                       });
    edge->locations.erase(locationOnDisk);
    if(!isLocatedOnThisProcessor(edge, id))
        edge->locations.emplace_back(LocationType::OnProcessor, id);
}

void locateToThisProcessorFromNowhere(edge_t* edge, int id){
    //  cout<<"locating from nowhere to proc "<<id <<" edge "; print_edge(edge);
    if(!isLocatedOnThisProcessor(edge, id))
        edge->locations.emplace_back(LocationType::OnProcessor, id);
}

void Event::fire(Cluster * cluster, queue<Event>& events){
    switch(this->type){
        case eventType::OnTaskStart:
            fireTaskStart(cluster, events);
            break;
        case eventType::OnTaskFinish:
            fireTaskFinish(cluster, events);
            break;
        case eventType::OnReadStart:
            fireReadStart(cluster, events);
            break;
        case eventType::OnReadFinish:
            fireReadFinish(cluster, events);
            break;
        case eventType::OnWriteStart:
            fireWriteStart(cluster, events);
            break;
        case eventType::OnWriteFinish:
            fireWriteFinish(cluster, events);
            break;
    }
}



