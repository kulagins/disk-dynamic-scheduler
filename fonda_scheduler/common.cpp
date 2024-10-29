//
// Created by kulagins on 26.05.23.
//

#include "../include/fonda_scheduler/common.hpp"

#include "graph.hpp"
#include "fonda_scheduler/dynSched.hpp"

bool Debug;

void takeOverChangesFromRunningTasks(json bodyjson, graph_t* currentWorkflow, vector<Assignment *> & assignments){
    if (bodyjson.contains("running_tasks") && bodyjson["running_tasks"].is_array()) {
        const auto &runningTasks = bodyjson["running_tasks"];
        //cout << "num running tasks: " << runningTasks.size() << " ";
        for (const auto &item: runningTasks) {
            // Check if the required fields (name, start, machine) exist in each object
            if (item.contains("name") && item.contains("start") && item.contains("machine")) {
                string name = item["name"];
                double start = item["start"];
                int machine = item["machine"];
                double realWork = item["work"];

                // Print the values of the fields to the console
                // std::cout << "Name: " << name << ", Start: " << start << ", Machine: " << machine << std::endl;
                name = trimQuotes(name);
                vertex_t *ver = findVertexByName(currentWorkflow, name);
                ver->visited = true;
                const vector<Assignment *>::iterator &it_assignm = std::find_if(assignments.begin(),
                                                                                assignments.end(),
                                                                                [name](Assignment *a) {
                                                                                    string tn = a->task->name;
                                                                                    transform(tn.begin(),
                                                                                              tn.end(),
                                                                                              tn.begin(),
                                                                                              [](unsigned char c) {
                                                                                                  return tolower(
                                                                                                          c);
                                                                                              });
                                                                                    return tn == name;
                                                                                });
                if (it_assignm != assignments.end() && realWork>(*it_assignm)->task->time) {
                    double realFinishTimeComputed = start + realWork /
                                                            (*it_assignm)->processor->getProcessorSpeed();
                    (*it_assignm)->startTime = start;
                    (*it_assignm)->finishTime = realFinishTimeComputed;
                }

            } else {
                cerr << "One or more fields missing in a running task object." << endl;
            }
        }

    } else {
        cout << "No running tasks found or wrong schema." << endl;
    }
}

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

string answerWithJson(vector<Assignment *> assignments, string workflowName){

    nlohmann::json jsonObject;

    jsonObject["id"] = workflowName;

    //"schedule": {

    nlohmann::json scheduleJson= nlohmann::json::array();

    // Serialize each Assignment object and add to the schedule object with task name as key
    for (const auto& assignment : assignments) {
        if(!assignment->task->visited)
            scheduleJson. push_back (assignment->toJson());
    }

    // Add the schedule to the main JSON object
    jsonObject["schedule"] = scheduleJson;

    return to_string(jsonObject);
}

void Cluster::printAssignment(){
    int counter =0;
    for (const auto &item: this->getProcessors()){
        if(item->isBusy){
            counter++;
            cout<<"Processor"<<counter<<"."<<endl;
            cout<<"\tMem: "<<item->getMemorySize()<<", Proc: "<<item->getProcessorSpeed()<<endl;

            vertex_t *assignedVertex = item->getAssignedTask();
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

