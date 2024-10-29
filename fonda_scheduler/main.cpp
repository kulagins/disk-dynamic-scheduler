#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <vector>
#include <map>
#include <filesystem>

#include "../extlibs/memdag/src/graph.hpp"
#include "../include/fonda_scheduler/dynSched.hpp"
#include "../include/fonda_scheduler/io/graphWeightsBuilder.hpp"

#include <chrono>
#include <cstring>
#include <csignal>



graph_t *currentWorkflow = NULL;
string currentName;
Cluster *currentCluster;
vector<Assignment *> currentAssignment;

double lastTimestamp = 0;
int currentAlgoNum = 0;
int updateCounter=0;
int delayCnt=0;
double lastAvgMem;

vector<Assignment *> currentAssignmentWithNoRecalculation;
bool isNoRecalculationStillValid= true;

std::shared_ptr<Http::Endpoint> endpoint;

void new_schedule(const Rest::Request &req, Http::ResponseWriter resp) {

 /*   for (auto &item: currentAssignment){
        delete item;
    }
    for (auto &item: currentAssignmentWithNoRecalculation){
        delete item;
    }

    if(currentWorkflow!=NULL)
        free_graph(currentWorkflow);
    if(currentCluster!=NULL){
     //   for (auto &item: currentCluster->getProcessors()){
      //      delete item;
     //   }
     //   delete currentCluster;

    } */
    currentAssignment.resize(0);
    currentAssignmentWithNoRecalculation.resize(0);
    updateCounter=delayCnt=0;
    isNoRecalculationStillValid=true;
    cout << fixed;

    graph_t *graphMemTopology;

    const string &basicString = req.body();
    json bodyjson;
    bodyjson = json::parse(basicString);
    // cout<<"Scheduler received a new workflow schedule request: "<<endl;//basicString<<endl;

    string workflowName = bodyjson["workflow"]["name"];
    workflowName = trimQuotes(workflowName);
    currentName = workflowName;
    int algoNumber = bodyjson["algorithm"].get<int>();
    cout << "new, algo " << algoNumber << " " <<currentName<<" ";

    string filename = "../input/";
    string suffix = "00";
    if (workflowName.substr(workflowName.size() - suffix.size()) == suffix) {
        filename += "generated/";//+filename;
    }
    filename += workflowName;

    filename += workflowName.substr(workflowName.size() - suffix.size()) == suffix ? ".dot" : "_sparse.dot";
    graphMemTopology = read_dot_graph(filename.c_str(), NULL, NULL, NULL);
    checkForZeroMemories(graphMemTopology);

    currentAlgoNum = algoNumber;
    Cluster *cluster = Fonda::buildClusterFromJson(bodyjson);
    cluster->setHomogeneousBandwidth(10000);
    //cluster->printProcessors();

    Fonda::fillGraphWeightsFromExternalSource(graphMemTopology, bodyjson);

    double maxMemReq = 0;
    vertex_t *vertex = graphMemTopology->first_vertex;
    while (vertex != nullptr) {
        maxMemReq = peakMemoryRequirementOfVertex(vertex) > maxMemReq?peakMemoryRequirementOfVertex(vertex):maxMemReq;
        vertex = vertex->next;
    }
    cout<<"MAX MEM REQ "<<maxMemReq<< " MMR  ";
    if(maxMemReq>cluster->getMemBiggestFreeProcessor()->getMemorySize()){
        cout<<"SCHEDULE IMPOSSIBLE"<<endl;
        resp.send(Http::Code::Not_Acceptable, "unacceptable schedule");
    }
    bool wasCorrect;
    double resultingMS;
    vector<Assignment *> assignments = runAlgorithm(algoNumber, graphMemTopology, cluster, workflowName, wasCorrect, resultingMS);

    if(wasCorrect){
    std::for_each(assignments.begin(), assignments.end(),[](Assignment* a){
      //  cout<<a->task->name<<" on "<<a->processor->id<< " "<<a->startTime<<" " <<a->finishTime<<endl;
    });
    const string answerJson =
            answerWithJson(assignments, workflowName);
    // cout<<answerJson<<endl;

    Http::Uri::Query &query = const_cast<Http::Uri::Query &>(req.query());
    query.as_str();

    currentWorkflow = graphMemTopology;
    currentCluster = cluster;
    currentAssignment = assignments;
    for (auto &item: currentAssignment){
        currentAssignmentWithNoRecalculation.emplace_back(new Assignment(item->task, item->processor, item->startTime, item->finishTime));
    }
    assert((*currentAssignment.begin())->startTime<= currentAssignment.at(currentAssignment.size()-1)->startTime);
    assert(currentAssignmentWithNoRecalculation.empty() ||
    (*currentAssignmentWithNoRecalculation.begin())->startTime<=
    currentAssignmentWithNoRecalculation.at(currentAssignmentWithNoRecalculation.size()-1)->startTime);
    resp.send(Http::Code::Ok, answerJson);
    }
    else
        resp.send(Http::Code::Not_Acceptable, "unacceptable schedule");

   // cout <<"RESULTING MS "<<resultingMS<<endl;
   cout<<endl;
}


void update(const Rest::Request &req, Http::ResponseWriter resp) {
   /* if(updateCounter>40){
        for (auto &item: currentAssignment){
            delete item;
        }
        for (auto &item: currentAssignmentWithNoRecalculation){
            delete item;
        }
        currentAssignment.resize(0);
        delete currentWorkflow;
        delete currentCluster;
        endpoint->shutdown();
        return;
    } */


    //try {
        updateCounter++;

        const string &basicString = req.body();
        json bodyjson;
        bodyjson = json::parse(basicString);
        cout << "Update ";

        double timestamp = bodyjson["time"];
        //cout << "timestamp " << timestamp << ", reason " << bodyjson["reason"] << " ";
        if (timestamp == lastTimestamp) {
            // cout << "time doesnt move";
        } else lastTimestamp = timestamp;

        string wholeReason = bodyjson["reason"];
        size_t pos = wholeReason.find("occupied");
        std::string partBeforeOccupied = wholeReason.substr(0, pos);
        std::string partAfterOccupied = wholeReason.substr(pos + 11);

        pos = partBeforeOccupied.find(": ");
        pos += 2;
        std::string nameOfTaskWithProblem = partBeforeOccupied.substr(pos);
        nameOfTaskWithProblem = trimQuotes(nameOfTaskWithProblem);

        pos = partAfterOccupied.find("on machine");
        string nameOfProblemCause = partAfterOccupied.substr(0, pos);
        nameOfProblemCause = trimQuotes(nameOfProblemCause);

        pos = wholeReason.find(':');
        string errorName = wholeReason.substr(0, pos);


        double newStartTime = bodyjson["new_start_time"];
        if (timestamp == newStartTime) {
            //cout<<endl<<"SAME TIME"<<endl;
        }

        vertex_t *taskWithProblem = findVertexByName(currentWorkflow, nameOfTaskWithProblem);

        if (currentWorkflow != NULL) {
            cout << errorName << " ";
            //cout << "num finished tasks: " << bodyjson["finished_tasks"].size() << " ";
            for (auto element: bodyjson["finished_tasks"]) {
                const string &elementWithQuotes = to_string(element);
                string elem_string = trimQuotes(elementWithQuotes);
                vertex_t *vertex = findVertexByName(currentWorkflow, elem_string);
                if (vertex == NULL)
                    printDebug("Update: not found vertex to set as finished: " + elem_string + "\n");
                else
                    //remove_vertex(currentWorkflow,vertex);
                    vertex->visited = true;
            }

            if (errorName == "MachineInUseException" || errorName == "TaskNotReadyException") {
                Assignment *assignmOfProblem = (*findAssignmentByName(currentAssignment, nameOfTaskWithProblem));
                assert(assignmOfProblem->startTime>=0);
                Assignment *assignmOfProblemCauser = (*findAssignmentByName(currentAssignment, nameOfProblemCause));
                bool isDelayPossible = isDelayPossibleUntil(assignmOfProblem,
                                                            newStartTime, currentAssignment, currentCluster);
                if (isDelayPossible && timestamp != newStartTime) {
                    delayOneTask(resp, bodyjson, nameOfTaskWithProblem, newStartTime, assignmOfProblem);
                    cout<<"yes "<< lastAvgMem <<" duration_of_algorithm 0.0 ";

                } else {
                    assignmOfProblemCauser->task->makespan = newStartTime;
                    assignmOfProblem->processor->readyTime = newStartTime;
                    completeRecomputationOfSchedule(resp, bodyjson, timestamp, taskWithProblem);
                }
                takeOverChangesFromRunningTasks(bodyjson, currentWorkflow, currentAssignmentWithNoRecalculation);
                delayEverythingBy(currentAssignmentWithNoRecalculation, assignmOfProblem, newStartTime-assignmOfProblem->startTime);
                std::sort(currentAssignmentWithNoRecalculation.begin(), currentAssignmentWithNoRecalculation.end(), [](Assignment* a, Assignment * b){
                    return a->finishTime<b->finishTime;
                });

            } else
                if (errorName == "InsufficientMemoryException") {
                    double d=0;
                try {
                    // Convert string to double using stod
                    d = std::stod(nameOfProblemCause);
                } catch (const std::invalid_argument& e) {
                    std::cerr << "Invalid argument: " << e.what() << '\n';
                    return;
                } catch (const std::out_of_range& e) {
                    std::cerr << "Out of range: " << e.what() << '\n';
                    return;
                }
               taskWithProblem->memoryRequirement=d;
               resp.send(Http::Code::Precondition_Failed, "");
               completeRecomputationOfSchedule(resp, bodyjson, timestamp, taskWithProblem);
               isNoRecalculationStillValid=false;

            } else if (errorName == "TookMuchLess") {
                //cout<<" too short ";
                vertex_t *vertex = findVertexByName(currentWorkflow, nameOfTaskWithProblem);
                if (vertex == nullptr)
                    printDebug("Update: not found vertex that finished early: " + nameOfTaskWithProblem);
                else {
                    //remove_vertex(currentWorkflow,vertex);
                    vertex->visited = true;
                    vertex->makespan = newStartTime;
                    vertex->assignedProcessor->readyTime = newStartTime;
                }
                assert(newStartTime == timestamp);
                completeRecomputationOfSchedule(resp, bodyjson, timestamp, vertex);
            }

        } else
            resp.send(Http::Code::Not_Acceptable, "No workflow has been scheduled yet.");

        std::sort(currentAssignment.begin(), currentAssignment.end(), [](Assignment* a, Assignment * b){
            return a->finishTime<b->finishTime;
        });
        assert((*currentAssignment.begin())->finishTime<= currentAssignment.at(currentAssignment.size()-1)->finishTime);
      //  assert(currentAssignmentWithNoRecalculation.empty() ||(*currentAssignmentWithNoRecalculation.begin())->startTime<= currentAssignmentWithNoRecalculation.at(currentAssignmentWithNoRecalculation.size()-1)->startTime);
        cout<<" no_recomputation: "<< (isNoRecalculationStillValid && !currentAssignmentWithNoRecalculation.empty() ? currentAssignmentWithNoRecalculation.at(currentAssignmentWithNoRecalculation.size()-1)->finishTime: -1)<<" ";
        cout << updateCounter << " " << delayCnt << endl;
 //   }
 //   catch (const std::exception& e) {
  //      std::cerr << "Exception: " << e.what() << std::endl;
 //       resp.send(Http::Code::Not_Acceptable, "Error during scheduling.");
  //  }
 //   catch(...){
  //      cout<<"Unknown error happened."<<endl;
  //      resp.send(Http::Code::Not_Acceptable, "Error during scheduling.");
  //  }
}

void handleSignal(int signal) {
  //  std::cout << "Interrupt signal (" << signal << ") received." << std::endl;
    // Perform cleanup or end the server here
    //exit(signal);

    std::cout << "Shutting down the server..." << std::endl;
    endpoint->shutdown();
}
int main(int argc, char *argv[]) {
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
    
    using namespace Rest;
    Debug = true;//true;
    std::cout << std::fixed ;//<< std::setprecision(3);

    int portFromArgs = argc>0 ? stoi(argv[1]): 9900;

    Router router;      // POST/GET/etc. route handler
    Port port(portFromArgs);    // port to listen on
    Address addr(Ipv4::any(), port);
    endpoint = std::make_shared<Http::Endpoint>(addr);
    auto opts = Http::Endpoint::options().maxRequestSize(9291456).threads(1).flags(Tcp::Options::ReuseAddr | Tcp::Options::ReusePort);
     endpoint->init(opts);

    /* routes! */
    Routes::Post(router, "/wf/:id/update", Routes::bind(&update));
    Routes::Post(router, "/wf/new/", Routes::bind(&new_schedule));


    endpoint->setHandler(router.handler());
    endpoint->serve();

    return 0;
}
void completeRecomputationOfSchedule(Http::ResponseWriter &resp, const json &bodyjson, double timestamp,
                                      vertex_t * vertexThatHasAProblem) {

    vector<Assignment *> assignments, tempAssignments;

    Cluster *updatedCluster = prepareClusterWithChangesAtTimestamp(bodyjson, timestamp, tempAssignments);

    if(vertexThatHasAProblem->visited){

    }
   // for ( auto &item: currentAssignment){
   //     delete item;
    //}
   // currentAssignment.resize(0);
 //   for (auto &item: currentCluster->getProcessors()){
//        delete item;
 //   }

    bool wasCorrect; double resMS;
    assignments = runAlgorithm(currentAlgoNum, currentWorkflow, updatedCluster, currentName, wasCorrect, resMS);
    const string answerJson =
            answerWithJson(assignments, currentName);


    //cout << "answered the update request with " << answerJson << endl;
    // std::for_each(assignments.begin(), assignments.end(),[](Assignment* a){
    //    cout<<a->task->name<<" on "<<a->processor->id<< "from "<<a->startTime<<" to "<<a->finishTime<<endl;
    // });
    //   cout<<"inserted temp"<<endl;
    assignments.insert(assignments.end(), tempAssignments.begin(), tempAssignments.end());

    std::sort(assignments.begin(), assignments.end(), [](Assignment* a, Assignment * b){
        return a->finishTime<b->finishTime;
    });
    std::for_each(assignments.begin(), assignments.end(),[](Assignment* a){
        //cout<<a->task->name<<" on "<<a->processor->id<< " " <<a->startTime<<" "<<a->finishTime<<endl;
    });

    currentAssignment = assignments;
    //delete currentCluster;
    currentCluster=updatedCluster;
    //cout<<updateCounter<<" "<<delayCnt<<endl;
    //||assignments.size() == tempAssignments.size()
    if (!wasCorrect ) {
       resp.send(Http::Code::Precondition_Failed, answerJson);
    } else {
        resp.send(Http::Code::Ok, answerJson);
    }
}

void delayOneTask(Http::ResponseWriter &resp, const json &bodyjson, string &nameOfTaskWithProblem, double newStartTime,
                  Assignment *assignmOfProblem) {
    vector<Assignment *> assignments = currentAssignment;
    assignmOfProblem->startTime = newStartTime;
    assignmOfProblem->finishTime =
            newStartTime + assignmOfProblem->task->makespan / assignmOfProblem->processor->getProcessorSpeed();

    assignmOfProblem = (*findAssignmentByName(assignments, nameOfTaskWithProblem));
    // cout << endl << "DELAY POSSIBLE! " << endl;
    delayCnt++;
    assignmOfProblem->startTime = newStartTime;
    assignmOfProblem->finishTime =
            newStartTime + assignmOfProblem->task->makespan / assignmOfProblem->processor->getProcessorSpeed();

    if (bodyjson.contains("running_tasks") && bodyjson["running_tasks"].is_array()) {
        const auto &runningTasks = bodyjson["running_tasks"];
       // cout << "num running tasks: " << runningTasks.size() << " ";
        for (const auto &item: runningTasks) {
            // Check if the required fields (name, start, machine) exist in each object
            if (item.contains("name") && item.contains("start") && item.contains("machine")) {
                string name = item["name"];
                name = trimQuotes(name);
                auto position = std::find_if(assignments.begin(),
                                                                       assignments.end(),
                                                                       [name](Assignment *a) {
                                                                           string tn = a->task->name;
                                                                           transform(
                                                                                   tn.begin(),
                                                                                   tn.end(),
                                                                                   tn.begin(),
                                                                                   [](unsigned char c) {
                                                                                       return tolower(
                                                                                               c);
                                                                                   });
                                                                           //cout<<"tn is "<<tn<<" name is "<<name<<endl;
                                                                           return tn ==
                                                                                  name;
                                                                       });

                if (position == assignments.end()) {
                    cout << endl<< "not found in assignments" << endl;
                    continue;
                } else {
                    assignments.erase(position);
                }

            }
        }
    }

    const string answerJson =
            answerWithJson(assignments, currentName);
    cout<< assignments.at(assignments.size()-1)->finishTime<<" ";
    //cout << "answ with " << answerJson << endl;
    resp.send(Http::Code::Ok, answerJson);
}


Cluster *
prepareClusterWithChangesAtTimestamp(const json &bodyjson, double timestamp, vector<Assignment *> &tempAssignments) {
    Cluster *updatedCluster = new Cluster(currentCluster);
    for (auto &processor: updatedCluster->getProcessors()) {
        processor->assignSubgraph(NULL);
        processor->isBusy = false;
        processor->readyTime = timestamp;
        processor->availableMemory = processor->getMemorySize();
        processor->availableBuffer = processor->communicationBuffer;
        processor->pendingInBuffer.clear();
        processor->pendingMemories.clear();
    }

    for ( auto &item: updatedCluster->readyTimesBuffers){
        for ( auto &item1: item){
            item1=timestamp;
        }
    }

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
                const vector<Assignment *>::iterator &it_assignm = std::find_if(currentAssignment.begin(),
                                                                                currentAssignment.end(),
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
                if (it_assignm != currentAssignment.end()) {
                    Processor *updatedProc = updatedCluster->getProcessorById((*it_assignm)->processor->id);
                    double realFinishTimeComputed = start + realWork /
                                                            updatedProc->getProcessorSpeed();
                    updatedProc->readyTime = realFinishTimeComputed;//(timestamp>(*it_assignm)->finishTime)? (timestamp+(*it_assignm)->task->time/(*it_assignm)->processor->getProcessorSpeed()): (*it_assignm)->finishTime;
                    ver->makespan = realFinishTimeComputed;
                    assert((*it_assignm)->task->name==ver->name);
                    double sumOut=0;

                    for (int i = 0; i < ver->out_degree; i++) {
                        sumOut += ver->out_edges[i]->weight;
                    }
                    updatedProc->availableMemory =
                           updatedProc->getMemorySize() -sumOut;
                    assert(updatedProc->availableMemory>=0);
                    updatedProc->assignSubgraph((*it_assignm)->task);
                    for (int j = 0; j <  ver->out_degree; j++) {
                       updatedProc->pendingMemories.insert(ver->out_edges[j]);

                    }
                    //assert( (*it_assignm)->startTime==start);
                    correctRtJJsOnPredecessors(updatedCluster, ver, updatedProc);
                    tempAssignments.emplace_back(new Assignment(ver, updatedProc, start,
                                                                start + ((*it_assignm)->finishTime) -
                                                                (*it_assignm)->startTime));
                } else {
                    cout << "running task not found in assignments " << name << endl;
                    for_each(currentAssignment.begin(), currentAssignment.end(), [](Assignment *a) {
                       // cout << a->task->name << " on " << a->processor->id << ", ";
                    });
                    //cout << endl;
                    if (ver != NULL) {
                        Processor *procOfTas = updatedCluster->getProcessorById(machine);
                        //procOfTas->readyTime = procOfTas->
                        //timestamp>( start +
                        //   ver->time / procOfTas->getProcessorSpeed())? (timestamp+ start +
                        //                                                    ver->time / procOfTas->getProcessorSpeed()) :(start +
                        //                                                ver->time / procOfTas->getProcessorSpeed());
                        procOfTas->availableMemory = procOfTas->getMemorySize() - ver->memoryRequirement;
                        assert(procOfTas->availableMemory>=0);
                        procOfTas=NULL;
                    } else {
                        cout << "task also not found in the workflow" << endl;
                    }
                }


            } else {
                cerr << "One or more fields missing in a running task object." << endl;
            }
        }

    } else {
        cout << "No running tasks found or wrong schema." << endl;
    }

    vertex_t *vertex = currentWorkflow->first_vertex;
    while (vertex != nullptr) {
        if (!vertex->visited) {
            for (int j = 0; j < vertex->in_degree; j++) {
                edge *incomingEdge = vertex->in_edges[j];
                vertex_t *predecessor = incomingEdge->tail;
                if (predecessor->visited) {
                    Processor *processorInUpdated = updatedCluster->getProcessorById(
                            predecessor->assignedProcessor->id);
                    auto foundInPendingMemsOfPredecessor = find_if(
                            predecessor->assignedProcessor->pendingMemories.begin(),
                            predecessor->assignedProcessor->pendingMemories.end(),
                            [incomingEdge](edge_t *edge) {
                                return edge->tail->name == incomingEdge->tail->name &&
                                       edge->head->name == incomingEdge->head->name;
                            });
                    if (foundInPendingMemsOfPredecessor == predecessor->assignedProcessor->pendingMemories.end()) {
                        auto foundInPendingBufsOfPredecessor = find_if(
                                predecessor->assignedProcessor->pendingInBuffer.begin(),
                                predecessor->assignedProcessor->pendingInBuffer.end(),
                                [incomingEdge](edge_t *edge) {
                                    return edge->tail->name == incomingEdge->tail->name &&
                                           edge->head->name == incomingEdge->head->name;
                                });
                        if (foundInPendingBufsOfPredecessor == predecessor->assignedProcessor->pendingInBuffer.end()) {
                            //cout<<"NOT FOUND EDGE"<< incomingEdge->tail->name << " -> " << incomingEdge->head->name <<" NOT FOUND ANYWHERE; INSERTING IN MEMS"<<endl;
                            if (processorInUpdated->availableMemory - incomingEdge->weight < 0) {
                                auto insertResult = processorInUpdated->pendingInBuffer.insert(incomingEdge);
                                if (insertResult.second) {
                                    processorInUpdated->availableBuffer -= incomingEdge->weight;
                                }
                                assert(processorInUpdated->availableMemory >= 0);
                            } else {
                                auto insertResult = processorInUpdated->pendingMemories.insert(incomingEdge);
                                if (insertResult.second) {
                                    processorInUpdated->availableMemory -= incomingEdge->weight;
                                }
                                assert(processorInUpdated->availableMemory >= 0);
                            }


                            // throw new runtime_error(
                            //        "edge " + incomingEdge->tail->name + " -> " + incomingEdge->head->name +
                            //       " found neither in pending mems, nor in bufs of processor " +
                            //      to_string(predecessor->assignedProcessor->id));

                        } else {

                            auto insertResult = processorInUpdated->pendingInBuffer.insert(incomingEdge);
                            if (insertResult.second) {
                                processorInUpdated->availableBuffer -= incomingEdge->weight;
                            }
                        }
                    } else {
                        auto insertResult = processorInUpdated->pendingMemories.insert(incomingEdge);
                        if (insertResult.second) {
                            processorInUpdated->availableMemory -= incomingEdge->weight;
                        }
                        assert(processorInUpdated->availableMemory >= 0);
                    }
                }
            }
        }

        vertex = vertex->next;
    }

   // vertex = currentWorkflow->first_vertex;
 //   while (vertex != nullptr) {
  //      Processor *updP = updatedCluster->getProcessorById(vertex->assignedProcessor->id);
   //     assert(updP->pendingMemories.size()==vertex->assignedProcessor->pendingMemories.size());
  //      vertex = vertex->next;
  //  }


    return updatedCluster;
}

vector<Assignment *>
runAlgorithm(int algorithmNumber, graph_t *graphMemTopology, Cluster *cluster, string workflowName, bool & wasCrrect, double &resultingMS) {
    vector<Assignment *> assignments;
    try {

        auto start = std::chrono::system_clock::now();
        double avgPeakMem = 0;
        switch (algorithmNumber) {
            case 1: {
                // print_graph_to_cout(graphMemTopology);
                double d = heuristic(graphMemTopology, cluster, 1, 0, assignments, avgPeakMem);
                resultingMS = d;
                wasCrrect =( d!=-1);
                cout << //workflowName << " " <<
                d << (wasCrrect ? " yes " : " no ") << avgPeakMem;
                break;
            }
            case 2: {
                double d = heuristic(graphMemTopology, cluster, 1, 1, assignments, avgPeakMem);
                wasCrrect =( d!=-1);resultingMS = d;
                cout << //workflowName << " " <<
                d << (wasCrrect ? " yes " : " no ") << avgPeakMem;
                break;
            }
            case 3: {
                double d = heuristic(graphMemTopology, cluster, 2, 0, assignments, avgPeakMem);
                wasCrrect =( d!=-1);resultingMS = d;
                cout << //workflowName << " " <<
                d << (wasCrrect ? " yes " : " no ") << avgPeakMem;
                break;
            }
            case 4: {
                double d = heuristic(graphMemTopology, cluster, 2, 1, assignments, avgPeakMem);
                wasCrrect =( d!=-1);resultingMS = d;
                cout << //workflowName << " " <<
                d << (wasCrrect ? " yes " : " no ") << avgPeakMem;
                break;
            }
            case 5: {
                double d = heuristic(graphMemTopology, cluster, 3, 0, assignments, avgPeakMem);
                wasCrrect =( d!=-1);resultingMS = d;
                cout << //workflowName << " " <<
                d << (wasCrrect ? " yes " : " no") << avgPeakMem;
                break;
            }
            case 6: {
                double d = heuristic(graphMemTopology, cluster, 3, 1, assignments, avgPeakMem);
                wasCrrect =( d!=-1);resultingMS = d;
                cout << //workflowName << " " <<
                d << (wasCrrect ? " yes " : " no ") << avgPeakMem;
                break;
            }
            case 7: {
                double ms =
                        0;
                wasCrrect = heft(graphMemTopology, cluster, ms, assignments, avgPeakMem);
                resultingMS = ms;
                cout << //workflowName << " " <<
                ms << (wasCrrect ? " yes" : " no") << " " << avgPeakMem<<endl;
                break;
            }
            default:
                cout << "UNKNOWN ALGORITHM" << endl;
        }
        lastAvgMem= avgPeakMem;
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        std::cout << " duration_of_algorithm " << elapsed_seconds.count()<<" ";// << endl;

    }
    catch (std::runtime_error &e) {
        cout << e.what() << endl;
        //return 1;
    }
    catch (...) {
        cout << "Unknown error happened" << endl;
    }
    return assignments;
}
