//
// Created by kulagins on 22.11.24.
//

#include "fonda_scheduler/dynSched.hpp"

#include <iterator>


double getFinishTimeWithPredecessorsAndBuffers(vertex_t *v, const Processor *pj, const Cluster *cluster, double & startTime) {
    double maxRtFromPredecessors=0;
    throw new runtime_error("to reimplement");

    for (int j = 0; j < v->in_degree; j++) {
        edge *incomingEdge = v->in_edges[j];
        vertex_t *predecessor = incomingEdge->tail;
        if (predecessor->assignedProcessorId != pj->id && predecessor->assignedProcessorId!=-1) {
            // Finish time from predecessor

            double rtFromPredecessor = predecessor->makespan + incomingEdge->weight / 1;
            // communication buffer
            double buffer = //cluster->readyTimesBuffers.at(predecessor->assignedProcessor->id).at(pj->id) +
                    //+incomingEdge->weight / bandwidth;
                    rtFromPredecessor = max(rtFromPredecessor, buffer);
            maxRtFromPredecessors = max(rtFromPredecessor, maxRtFromPredecessors);
        }
    }

    // cout<<"pj ready "<<pj->readyTime<<" rt pred "<<maxRtFromPredecessors<<endl;
    startTime = max(pj->readyTimeCompute, maxRtFromPredecessors);
    double finishtime = startTime + v->time/pj->getProcessorSpeed();
    return finishtime;
}

vertex_t *
getLongestPredecessorWithBuffers(vertex_t *child, const Cluster *cluster, double &latestPredecessorFinishTime) {
    double maxRtFromPredecessors = 0;
    vertex_t *longestPredecessor;
    /*  latestPredecessorFinishTime = 0;
      for (int j = 0; j < child->in_degree; j++) {
          edge *incomingEdge = child->in_edges[j];
          vertex_t *predecessor = incomingEdge->tail;
          if(predecessor->assignedProcessor == NULL){
              throw new runtime_error("predecessor "+predecessor->name+" not assigned");
          }
          double rtFromPredecessor;
          if (predecessor->assignedProcessor->id != child->assignedProcessor->id ) {
              // Finish time from predecessor
              rtFromPredecessor = predecessor->makespan + incomingEdge->weight / bandwidth;
              // communication buffer
              double buffer = cluster->readyTimesBuffers.at(predecessor->assignedProcessor->id).at(child->assignedProcessor->id) +
                              +incomingEdge->weight / bandwidth;
              rtFromPredecessor = max(rtFromPredecessor, buffer);

          }
          else{
              rtFromPredecessor = predecessor->makespan;
          }
          maxRtFromPredecessors = max(rtFromPredecessor, maxRtFromPredecessors);
          if (maxRtFromPredecessors > latestPredecessorFinishTime) {
              latestPredecessorFinishTime = maxRtFromPredecessors;
              longestPredecessor = predecessor;
          }
      }

  */
    return longestPredecessor;
}

double exponentialTransformation(double x, double median, double scaleFactor) {
    //cout<<x;
    if (x < median) {
        // cout<<" "<<x - (median - x) * scaleFactor<<endl;
        return x - (median - x) * scaleFactor;
    } else {
        //  cout<<" "<<x + (x - median) * scaleFactor<<endl;
        return x + (x - median) * scaleFactor;
    }

}

void applyExponentialTransformationWithFactor(double factor, graph_t* graph){
    vector<double> memories;
    vector<double> makespans;
    for(int i=1; i<graph->next_vertex_index;i++){
        memories.emplace_back(graph->vertices_by_id[i]->memoryRequirement);
        makespans.emplace_back(graph->vertices_by_id[i]->time);
    }
    std::sort(makespans.begin(), makespans.end());
    double medianMs;
    if (makespans.size() % 2 == 0) {
        medianMs = (makespans[makespans.size() / 2 - 1] + makespans[makespans.size() / 2]) / 2.0;
    } else {
        medianMs = makespans[makespans.size() / 2];
    }

    std::sort(memories.begin(), memories.end());
    double medianMem;
    if (memories.size() % 2 == 0) {
        medianMem = (memories[memories.size() / 2 - 1] + memories[memories.size() / 2]) / 2.0;
    } else {
        medianMem = memories[memories.size() / 2];
    }

    for(int i=1; i<graph->next_vertex_index;i++){
        double mem_exp = exponentialTransformation(graph->vertices_by_id[i]->memoryRequirement, medianMem, factor);
        cout<<graph->vertices_by_id[i]->memoryRequirement;
        graph->vertices_by_id[i]->memoryRequirement = (mem_exp<0? 0.1:mem_exp );
        cout<<" "<<graph->vertices_by_id[i]->memoryRequirement<<endl;
        cout<<graph->vertices_by_id[i]->time;
        double ms_exp = exponentialTransformation(graph->vertices_by_id[i]->time, medianMs, factor);
        graph->vertices_by_id[i]->time =  (ms_exp<0? 0.1:ms_exp );
        cout<<" "<<graph->vertices_by_id[i]->time<<endl;
    }
}

double heuristic(graph_t *graph, Cluster *cluster, int bottomLevelVariant, int evictionVariant,
                 vector<Assignment *> &assignments, double &avgPeakMem) {
    /* vector<pair<Processor *, double>> peakMems;
     vector<pair<vertex_t *, double>> ranks = calculateBottomLevels(graph, bottomLevelVariant);

     removeSourceAndTarget(graph, ranks);

     sort(ranks.begin(), ranks.end(),
          [](pair<vertex_t *, double> a, pair<vertex_t *, double> b) { return a.second > b.second; });

     bool evistBiggestFirst = false;
     if (evictionVariant == 1) {
         evistBiggestFirst = true;
     }

     double maxFinishTime = 0;
     double sumPeakMems=0;
     for (const pair<vertex_t *, double> item: ranks) {

         vertex_t *vertexToAssign = item.first;
         if( vertexToAssign->name== "GRAPH_TARGET" ||vertexToAssign->name== "GRAPH_SOURCE") continue;
         if(vertexToAssign->visited) continue;
         printInlineDebug("assign vertex "+vertexToAssign->name);

         double minFinishTime= numeric_limits<double>::max();
         double startTimeToMinFinish;
         vector<edge_t * > bestEdgesToKick;
         double peakMemOnBestp=0;
         int bestProcessorId;

         for (const auto &p: cluster->getProcessors()){
             assert(p->availableMemory >= 0);
             double finishTime, startTime;

             bool isValid = true;
             double peakMem = 0;
             vector<edge_t * > edgesToKick = {};//tentativeAssignment(vertexToAssign, p, cluster, finishTime, startTime,
                                                                 //isValid, peakMem, evistBiggestFirst);

             if(isValid && minFinishTime>finishTime){
                 bestProcessorId = p->id;
                 minFinishTime = finishTime;
                 startTimeToMinFinish = startTime;
                 bestEdgesToKick = edgesToKick;
                 peakMemOnBestp = peakMem;
             }
         }
         if(minFinishTime==numeric_limits<double>::max() ){
             cout<<"Failed to find a processor for "<<vertexToAssign->name<<", FAIL"<<" ";
             cluster->printProcessors();
             assignments.resize(0);
             return -1;
         }

         Processor *procToChange = cluster->getProcessorById(bestProcessorId);
         printDebug("Really assigning to proc "+ to_string(procToChange->id));
         doRealAssignmentWithMemoryAdjustments(cluster, minFinishTime, bestEdgesToKick, vertexToAssign, procToChange);
         Assignment * assignment = new Assignment(vertexToAssign, procToChange, startTimeToMinFinish, minFinishTime);
         assignments.emplace_back(assignment);
         sumPeakMems+=peakMemOnBestp;

        //assignments.emplace_back(new Assignment(vertexToAssign, new Processor(procToChange), minFinishTime,0));
        maxFinishTime = max(maxFinishTime, minFinishTime);
        if(procToChange->peakMemConsumption<peakMemOnBestp) procToChange->peakMemConsumption= peakMemOnBestp;
         bestEdgesToKick.resize(0);
     }
     avgPeakMem = sumPeakMems/graph->number_of_vertices;
    /// for (const auto &item: cluster->getProcessors()){
     //    avgPeakMem+=item->peakMemConsumption;
    // }
    // avgPeakMem/=cluster->getNumberProcessors();

     std::sort(assignments.begin(), assignments.end(), [](Assignment* a, Assignment * b){
         return a->finishTime<b->finishTime;
     });
     return maxFinishTime; */
}

void doRealAssignmentWithMemoryAdjustments(Cluster *cluster, double futureReadyTime, const vector<edge_t*> & edgesToKick,
                                           vertex_t *vertexToAssign,  Processor *procToChange) {
    /* throw new runtime_error("to reimplement");
     vertexToAssign->assignedProcessor= procToChange;

     set<edge_t *>::iterator it;
     edge_t *pEdge;
     kickEdgesThatNeededToKickedToFreeMemForTask(edgesToKick, procToChange);

     //remove pending memories incoming in this vertex, release their memory
     it = procToChange->pendingMemories.begin();
     while (it != procToChange->pendingMemories.end()) {
         pEdge = *it;
         if (pEdge->head->name == vertexToAssign->name )  {
             procToChange->availableMemory += pEdge->weight;
             it = procToChange->pendingMemories.erase(it);
         } else {
             if (pEdge->tail->name == vertexToAssign->name ) {
                 cout<<"Vertex on tail, something wrong!"<<endl;
             }
             it++;
         }
     }

     //correct rt_j,j' for all predecessor buffers
     //correctRtJJsOnPredecessors(cluster, vertexToAssign, procToChange);

     //compute available mem and add outgoing edges to pending memories
     double availableMem = procToChange->getAvailableMemory();
     for (int j = 0; j < vertexToAssign->out_degree; j++) {
        procToChange->pendingMemories.insert(vertexToAssign->out_edges[j]);
        availableMem-= vertexToAssign->out_edges[j]->weight;
     }
     availableMem = removeInputPendingEdgesFromEverywherePendingMemAndBuffer(cluster, vertexToAssign, procToChange,
                                                                             availableMem);
     assert(availableMem>=0);
     printDebug("to proc " + to_string(procToChange->id) + " remaining mem " + to_string(availableMem) + "ready time " +
                to_string(futureReadyTime) + "\n");
     assert(availableMem <= (procToChange->getMemorySize() + 0.001));
     procToChange->setAvailableMemory(availableMem);
     procToChange->readyTimeCompute = futureReadyTime;
     vertexToAssign->makespan = futureReadyTime;
     */
}



double removeInputPendingEdgesFromEverywherePendingMemAndBuffer(const Cluster *cluster, const vertex_t *vertexToAssign,
                                                                Processor *procToChange, double availableMem) {
    throw new runtime_error("not implemented");
}

void kickEdgesThatNeededToKickedToFreeMemForTask(const vector<edge_t *> &edgesToKick, Processor *procToChange) {

    throw  new runtime_error("not implemented");
}



vector<vertex_t*> getReadyTasks(graph_t *graph){
    vector<vertex_t*> readies;
    vertex_t *vertex = graph->first_vertex;
    while (vertex != NULL) {
        if(vertex->in_degree==0){
            readies.emplace_back(vertex);
        }
        vertex = vertex->next;
    }
    return readies;
}

vector<edge_t*> evict(vector<edge_t*>  pendingMemories, double &stillTooMuch, bool evictBiggestFirst) {

    vector<edge_t*> kickedOut;

    if (!pendingMemories.empty())
        assert((*pendingMemories.begin())->weight <= (*pendingMemories.rbegin())->weight);
    //printInlineDebug(" kick out ");
    if (!evictBiggestFirst) {
        auto it = pendingMemories.begin();
        while (it != pendingMemories.end() && stillTooMuch < 0) {
            stillTooMuch += (*it)->weight;
            kickedOut.emplace_back(*it);
            it = pendingMemories.erase(it);

        }
    } else {
        for (auto it = pendingMemories.end();
             it != pendingMemories.begin() && stillTooMuch < 0;) {
            it--;
            stillTooMuch += (*it)->weight;
            kickedOut.emplace_back(*it);
            it = pendingMemories.erase(it);
        }
    }
    return kickedOut;
}
bool heft(graph_t *G, Cluster *cluster, double & makespan, vector<Assignment*> &assignments, double & avgPeakMem) {
    /*
    Cluster *clusterToCheckCorrectness = new Cluster(cluster);
    vector<pair<vertex_t *, double>> ranks = calculateBottomLevels(G, 1);
    sort(ranks.begin(), ranks.end(),[](pair<vertex_t *, double > a, pair<vertex_t *, double > b){ return  a.second> b.second;});

    bool errorsHappened = false;
    //choose the best processor
    vector<Processor *> assignedProcessor(G->number_of_vertices, NULL); // -1 indicates unassigned
    double maxFinishTime=0;
    for (const pair<vertex_t *, double > item: ranks) {
        int i = 0;
            vertex_t *vertex = item.first;
            printInlineDebug("assign vertex "+ vertex->name);
            double bestTime =  numeric_limits<double>::max();
            double startTimeToBestFinish;
            Processor * bestProc = NULL;
            for (size_t j = 0; j < cluster->getNumberProcessors(); ++j) {
                Processor *tentativeProcessor = cluster->getProcessors().at(j);
                double startTime;
                double finishTime = getFinishTimeWithPredecessorsAndBuffers(vertex, tentativeProcessor, cluster, startTime);
                if ( finishTime <= bestTime) {
                     bestTime = finishTime;
                     bestProc = tentativeProcessor;
                     startTimeToBestFinish = startTime;
                }
            }
            if(bestProc==NULL){
                return  numeric_limits<double>::max();
            }
            assignedProcessor[i] = bestProc;
            vertex->assignedProcessor = bestProc;
            bestProc->readyTime = bestTime;

            Assignment * assignment = new Assignment(vertex, bestProc, startTimeToBestFinish, bestTime);
            assignments.emplace_back(assignment);

            Processor *processorToCheckCorrectness = clusterToCheckCorrectness->getProcessorById(bestProc->id);
            double FT = numeric_limits<double>::max();
            bool isValid= true;
            double peakMem=0, startTime;
            vector<edge_t * > bestEdgesToKick;
            vector<edge_t * > edgesToKick = tentativeAssignment(vertex, processorToCheckCorrectness, cluster, FT,
                                                                startTime, isValid, peakMem);

            if(FT ==numeric_limits<double>::max()){
                errorsHappened=true;
            }
            bestProc->peakMemConsumption=peakMem;
            if(!isValid){
                errorsHappened= true;
            }
            if(!errorsHappened){
                doRealAssignmentWithMemoryAdjustments(clusterToCheckCorrectness,FT,edgesToKick,
                                                      vertex, processorToCheckCorrectness);
            }


        printDebug(" to "+ to_string(bestProc->id)+ " ready at "+ to_string(bestProc->readyTime));
            correctRtJJsOnPredecessors(cluster, vertex, bestProc);

            vertex->makespan = bestTime;
            maxFinishTime = max(maxFinishTime, bestTime);
            i++;

    }
    for (const auto &item: cluster->getProcessors()){
        avgPeakMem+=item->peakMemConsumption;
    }
    avgPeakMem/=cluster->getNumberProcessors();
    makespan = maxFinishTime;
    return !errorsHappened;
     */
}

bool isDelayPossibleUntil(Assignment *assignmentToDelay, double newStartTime, vector<Assignment *> assignments,
                          Cluster *cluster) {
    vertex_t *vertexToDelay = assignmentToDelay->task;
    double finishTimeOfDelay = assignmentToDelay->finishTime;
    auto processorOfDelay = cluster->getProcessorById(vertexToDelay->assignedProcessorId);
    double newFinishTime = newStartTime + vertexToDelay->time / processorOfDelay->getProcessorSpeed();
    vertexToDelay->makespan= newFinishTime;

    for (int j = 0; j < vertexToDelay->out_degree; j++) {
        assert(vertexToDelay->out_edges[j]->tail == vertexToDelay);
        vertex_t *child = vertexToDelay->out_edges[j]->head;
        auto childAssignment = findAssignmentByName(assignments, child->name);
        if(childAssignment== assignments.end()){
            cout<<"NOT FOUND CHILD ASSIGNEMNT for child "<< child->name<<" , is finished or running? "<<child->visited<<" "<<endl;
            continue;
        }
        auto startOfChild = (*childAssignment)->startTime;
        double latestPredFinishTime = 0;
        auto longestPredecessor = getLongestPredecessorWithBuffers(child, cluster, latestPredFinishTime);

        bool areWeTheLongestPredecessor = longestPredecessor == vertexToDelay;
        bool doWeBecomeTheLongestPredecessor = latestPredFinishTime <= newFinishTime;
        bool doWeFinishAfterTaskStarts;
        if((*childAssignment)->processor->id== assignmentToDelay->processor->id)
            doWeFinishAfterTaskStarts = newFinishTime>startOfChild;
        else{
            double completeFinishTime = newFinishTime + vertexToDelay->out_edges[j]->weight / 1;//cluster->getBandwidth();
            doWeFinishAfterTaskStarts = completeFinishTime>startOfChild;
        }

        if ((areWeTheLongestPredecessor && doWeFinishAfterTaskStarts) || doWeBecomeTheLongestPredecessor) {
            return false;
        }

    }

    std::vector<Assignment *> allAfterDelayedOne;
    // Find all elements greater than 5
    std::copy_if(assignments.begin(), assignments.end(), std::back_inserter(allAfterDelayedOne),
                 [processorOfDelay, finishTimeOfDelay](Assignment *assignment) {
                     return assignment->processor->id == processorOfDelay->id &&
                            assignment->startTime > finishTimeOfDelay;
                 });
    sort(allAfterDelayedOne.begin(), allAfterDelayedOne.end(), [](Assignment *ass1, Assignment *ass2) { return ass1->startTime<ass2->startTime; });

    if (!allAfterDelayedOne.empty() && (*allAfterDelayedOne.begin())->startTime <= newFinishTime) {
        return false;
    }
    return true;
}

std::vector<Assignment*>::iterator findAssignmentByName(vector<Assignment *> &assignments, string name) {
    string nameTL = name;
    std::transform(
            nameTL.begin(),
            nameTL.end(),
            nameTL.begin(),
            [](unsigned char c) {
                return std::tolower(
                        c);
            });
    auto it = std::find_if(assignments.begin(),
                           assignments.end(),
                           [nameTL](Assignment *a) {
                               string tn = a->task->name;
                               std::transform(
                                       tn.begin(),
                                       tn.end(),
                                       tn.begin(),
                                       [](unsigned char c) {
                                           return std::tolower(
                                                   c);
                                       });

                               return tn ==
                                      nameTL;
                           });
    if (it != assignments.end()) {
        return it;
    } else {
        cout << "no assignment found for " << name << endl;
        std::for_each(assignments.begin(), assignments.end(),[](Assignment* a){cout<<a->task->name<<" on "<<a->processor->id<<", ";});
        cout<<endl;
        throw runtime_error("find assignment by name failed, no assignment found for "+name);
    }
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
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
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



void new_schedule(const Rest::Request &req, Http::ResponseWriter resp) {


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

    //cluster->printProcessors();

    Fonda::fillGraphWeightsFromExternalSource(graphMemTopology, bodyjson, workflowName, cluster);

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
                assignmOfProblem->processor->readyTimeCompute = newStartTime;
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
                //TODO FIND IN CLUSTRE BY ID
                //vertex->assignedProcessor->readyTimeCompute = newStartTime;
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
    /*Cluster *updatedCluster = new Cluster(currentCluster);
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
                    assert(updatedProc->availableMemory >= 0);
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
                        assert(procOfTas->availableMemory >= 0);
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
     */
}
