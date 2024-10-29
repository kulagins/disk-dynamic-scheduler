#include "fonda_scheduler/dynSched.hpp"

#include <iterator>

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

bool isDelayPossibleUntil(Assignment *assignmentToDelay, double newStartTime, vector<Assignment *> assignments,
                            Cluster *cluster) {
    vertex_t *vertexToDelay = assignmentToDelay->task;
    double finishTimeOfDelay = assignmentToDelay->finishTime;
    auto processorOfDelay = vertexToDelay->assignedProcessor;
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
           double completeFinishTime = newFinishTime + vertexToDelay->out_edges[j]->weight / cluster->getBandwidth();
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

graph_t *convertToNonMemRepresentation(graph_t *withMemories, map<int, int> &noMemToWithMem) {
    enforce_single_source_and_target(withMemories);
    graph_t *noNodeMemories = new_graph();

    for (vertex_t *vertex = withMemories->source; vertex; vertex = next_vertex_in_sorted_topological_order(withMemories,
                                                                                                           vertex,
                                                                                                           &sort_by_increasing_top_level)) {
        vertex_t *invtx = new_vertex(noNodeMemories, vertex->name + "-in", vertex->time, NULL);
        noMemToWithMem.insert({invtx->id, vertex->id});
        if (!noNodeMemories->source) {
            noNodeMemories->source = invtx;
        }
        vertex_t *outvtx = new_vertex(noNodeMemories, vertex->name + "-out", 0.0, NULL);
        noMemToWithMem.insert({outvtx->id, vertex->id});
        edge_t *e = new_edge(noNodeMemories, invtx, outvtx, vertex->memoryRequirement, NULL);
        noNodeMemories->target = outvtx;

        for (int i = 0; i < vertex->in_degree; i++) {
            edge *inEdgeOriginal = vertex->in_edges[i];
            string expectedName = inEdgeOriginal->tail->name + "-out";
            vertex_t *outVtxOfCopiedInVtxOfEdge = findVertexByName(noNodeMemories, expectedName);

            if (outVtxOfCopiedInVtxOfEdge == NULL) {
                print_graph_to_cout(noNodeMemories);
                outVtxOfCopiedInVtxOfEdge = findVertexByName(noNodeMemories, expectedName);
                cout << "expected: " << expectedName << endl;
                throw std::invalid_argument(" no vertex found for expected name.");

            }
            edge_t *e_new = new_edge(noNodeMemories, outVtxOfCopiedInVtxOfEdge, invtx, inEdgeOriginal->weight, NULL);
        }
    }

    return noNodeMemories;
}


bool heft(graph_t *G, Cluster *cluster, double & makespan, vector<Assignment*> &assignments, double & avgPeakMem) {
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
            vector<edge_t * > edgesToKick = tentativeAssignmentDespiteMemory(vertex,processorToCheckCorrectness,cluster,FT, startTime, isValid, peakMem);

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
}

double calculateSimpleBottomUpRank(vertex_t *task) {

    double maxCost = 0.0;
    for (int j = 0; j < task->out_degree; j++) {
        double communicationCost = task->out_edges[j]->weight;
        double successorCost = calculateSimpleBottomUpRank(task->out_edges[j]->head);
        double cost = communicationCost + successorCost;
        maxCost = max(maxCost, cost);
    }
    double retur = task->time + maxCost;
    return retur;
}

double calculateBLCBottomUpRank(vertex_t *task) {

    double maxCost = 0.0;
    for (int j = 0; j < task->out_degree; j++) {
        double communicationCost = task->out_edges[j]->weight;
        double successorCost = calculateBLCBottomUpRank(task->out_edges[j]->head);
        double cost = communicationCost + successorCost;
        maxCost = max(maxCost, cost);
    }
    double simpleBl = task->time + maxCost;

    double maxInputCost=0.0;
    for (int j = 0; j < task->in_degree; j++) {
        double communicationCost = task->in_edges[j]->weight;
        maxInputCost = max(maxInputCost, communicationCost);
    }
    double retur = simpleBl + maxInputCost;
    return retur;
}

std::vector < std::pair< vertex_t *, double> >  calculateMMBottomUpRank(graph_t * graphWMems){

    map<int, int> noMemToWithMem;
    graph_t *graph = convertToNonMemRepresentation(graphWMems, noMemToWithMem);
   // print_graph_to_cout(graph);

    SP_tree_t *sp_tree = NULL;
    graph_t *sp_graph = NULL;

    enforce_single_source_and_target(graph);
    sp_tree = build_SP_decomposition_tree(graph);
    if (sp_tree) {
        sp_graph = graph;
    } else {
        sp_graph = graph_sp_ization(graph);
        sp_tree = build_SP_decomposition_tree(sp_graph);
    }


    std::vector < std::pair< vertex_t *, int> > scheduleOnOriginal;

    if (sp_tree) {
        vertex_t **schedule = compute_optimal_SP_traversal(sp_graph, sp_tree);

        for(int i=0; i<sp_graph->number_of_vertices; i++){
            vertex_t *vInSp = schedule[i];
            //cout<<vInSp->name<<endl;
            const map<int, int>::iterator &it = noMemToWithMem.find(vInSp->id);
            if (it != noMemToWithMem.end()) {
                vertex_t *vertexWithMem = graphWMems->vertices_by_id[(*it).second];
                if (std::find_if(scheduleOnOriginal.begin(), scheduleOnOriginal.end(),
                                 [vertexWithMem](std::pair<vertex_t *, int> p) {
                                     return p.first->name == vertexWithMem->name;
                                 }) == scheduleOnOriginal.end()) {
                    scheduleOnOriginal.emplace_back(vertexWithMem, sp_graph->number_of_vertices-i);// TODO: #vertices - i?
                }
            }
        }

    } else {
        throw new runtime_error("No tree decomposition");
    }
    delete sp_tree;
    delete sp_graph;
    //delete graph;


    std::vector<std::pair<vertex_t*, double>> double_vector;

    // Convert each pair from (vertex_t*, int) to (vertex_t*, double)
    for (const auto& pair : scheduleOnOriginal) {
        double_vector.push_back({pair.first, static_cast<double>(pair.second)});
    }

    return double_vector;
}


vector<edge_t*> evict(vector<edge_t*>  pendingMemories, double &currentlyAvailableBuffer, double &stillTooMuch, bool evictBiggestFirst) {

    vector<edge_t*> kickedOut;

    if (!pendingMemories.empty())
        assert((*pendingMemories.begin())->weight <= (*pendingMemories.rbegin())->weight);
    //printInlineDebug(" kick out ");
    if (!evictBiggestFirst) {
        auto it = pendingMemories.begin();
        while (it != pendingMemories.end() && stillTooMuch < 0) {
            stillTooMuch += (*it)->weight;
            currentlyAvailableBuffer -= (*it)->weight;
            kickedOut.emplace_back(*it);
            it = pendingMemories.erase(it);

        }
    } else {
        for (auto it = pendingMemories.end();
             it != pendingMemories.begin() && stillTooMuch < 0;) {
            it--;
            stillTooMuch += (*it)->weight;
            currentlyAvailableBuffer -= (*it)->weight;
            kickedOut.emplace_back(*it);
            it = pendingMemories.erase(it);
        }
    }
    return kickedOut;
}

double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere3Part(const vertex_t *v, const Processor *pj) {
    double Res = pj->availableMemory - v->memoryRequirement;

    //sum c_uv not on pj
    for (int j = 0; j < v->in_degree; j++) {
        edge *incomingEdge = v->in_edges[j];
        vertex_t *predecessor = incomingEdge->tail;
        if (predecessor->assignedProcessor->id != pj->id) {
            Res -= incomingEdge->weight;
        }
    }

    // sum c_uw on successors
    for (int j = 0; j < v->out_degree; j++) {
        edge *outgoindEdge = v->out_edges[j];
        Res -= outgoindEdge->weight;
    }
    return Res;
}

double howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(const vertex_t *v, const Processor *pj) {
    assert(pj->availableMemory>=0);
    double Res = pj->availableMemory - peakMemoryRequirementOfVertex(v);
    return Res;
}


double getFinishTimeWithPredecessorsAndBuffers(vertex_t *v, const Processor *pj, const Cluster *cluster, double & startTime) {
    double maxRtFromPredecessors=0;
    double bandwidth = cluster->getBandwidth();
    for (int j = 0; j < v->in_degree; j++) {
        edge *incomingEdge = v->in_edges[j];
        vertex_t *predecessor = incomingEdge->tail;
        if (predecessor->assignedProcessor->id != pj->id && predecessor->assignedProcessor!=NULL) {
            // Finish time from predecessor

            double rtFromPredecessor = predecessor->makespan + incomingEdge->weight / bandwidth;
            // communication buffer
            double buffer = cluster->readyTimesBuffers.at(predecessor->assignedProcessor->id).at(pj->id) +
                            +incomingEdge->weight / bandwidth;
            rtFromPredecessor = max(rtFromPredecessor, buffer);
            maxRtFromPredecessors = max(rtFromPredecessor, maxRtFromPredecessors);
        }
    }

   // cout<<"pj ready "<<pj->readyTime<<" rt pred "<<maxRtFromPredecessors<<endl;
    startTime = max(pj->readyTime, maxRtFromPredecessors);
    double finishtime = startTime + v->time/pj->getProcessorSpeed();
    return finishtime;
}

vertex_t *
getLongestPredecessorWithBuffers(vertex_t *child, const Cluster *cluster, double &latestPredecessorFinishTime) {
    double maxRtFromPredecessors = 0;
    double bandwidth = cluster->getBandwidth();
    vertex_t *longestPredecessor;
    latestPredecessorFinishTime = 0;
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


    return longestPredecessor;
}


double heuristic(graph_t *graph, Cluster *cluster, int bottomLevelVariant, int evictionVariant,
                 vector<Assignment *> &assignments, double &avgPeakMem) {
    vector<pair<Processor *, double>> peakMems;
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
            assert(p->availableMemory>=0);
            double finishTime, startTime;

            bool isValid = true;
            double peakMem = 0;
            vector<edge_t * > edgesToKick = tentativeAssignmentDespiteMemory(vertexToAssign, p, cluster, finishTime, startTime,
                                                                  isValid, peakMem, evistBiggestFirst);

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
    return maxFinishTime;
}

void doRealAssignmentWithMemoryAdjustments(Cluster *cluster, double futureReadyTime, const vector<edge_t*> & edgesToKick,
                                           vertex_t *vertexToAssign,  Processor *procToChange) {
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
    correctRtJJsOnPredecessors(cluster, vertexToAssign, procToChange);

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
    procToChange->readyTime = futureReadyTime;
    vertexToAssign->makespan = futureReadyTime;
}

void correctRtJJsOnPredecessors(Cluster *cluster, const vertex_t *vertexToAssign, const Processor *procToChange) {
    for(int i=0; i < vertexToAssign->in_degree; i++){
        vertex_t *predecessor = vertexToAssign->in_edges[i]->tail;
        if(predecessor->assignedProcessor==NULL)
            cout<<"predecessor not assigned proc! "<<endl;
        if(predecessor->assignedProcessor->id!=procToChange->id){
            cluster->readyTimesBuffers.at(predecessor->assignedProcessor->id).at(procToChange->id) += vertexToAssign->in_edges[i]->weight/cluster->getBandwidth();
                    }

    }
}

double removeInputPendingEdgesFromEverywherePendingMemAndBuffer(const Cluster *cluster, const vertex_t *vertexToAssign,
                                                                Processor *procToChange, double availableMem) {
    for (int j = 0; j < vertexToAssign->in_degree; j++) {
        edge *edge = vertexToAssign->in_edges[j];
        for (auto processor: cluster->getProcessors()) {
            for (auto it = processor->pendingMemories.begin(); it != processor->pendingMemories.end();) {
                if ((*it)->tail->name == edge->tail->name && (*it)->head->name == edge->head->name) {
                    if(processor->id == procToChange->id){
                        availableMem += edge->weight;
                    }
                    else processor->availableMemory+= edge->weight;
                    it = processor->pendingMemories.erase(it);
                } else {
                    ++it;
                }
            }

            auto it = find_if(processor->pendingInBuffer.begin(), processor->pendingInBuffer.end(),
                              [](edge_t *edge) { return 0; });
            if (it != processor->pendingInBuffer.end()) {
                processor->availableBuffer += edge->weight;
                processor->pendingInBuffer.erase(it);
            }
            assert(processor->availableBuffer <= processor->communicationBuffer);
            //assert(processor->availableMemory <= (processor->getMemorySize()+0.001));
        }
    }
    return availableMem;
}

void kickEdgesThatNeededToKickedToFreeMemForTask(const vector<edge_t *> &edgesToKick, Processor *procToChange) {

    for (auto &edgeToKick: edgesToKick){
        auto foundInPendingMems = find_if(procToChange->pendingMemories.begin(), procToChange->pendingMemories.end(),
                                    [edgeToKick](edge_t *edge) {
                                        return edge->tail->name == edgeToKick->tail->name &&
                                               edge->head->name == edgeToKick->head->name;
                                    });
        if (foundInPendingMems != procToChange->pendingMemories.end() )  {
            printDebug("really kicking edge " +(*foundInPendingMems)->tail->name+"->" +(*foundInPendingMems)->head->name+", ");
            procToChange->availableBuffer -= (*foundInPendingMems)->weight;
            procToChange->pendingInBuffer.insert(*foundInPendingMems);
            procToChange->availableMemory+=(*foundInPendingMems)->weight;
            procToChange->pendingMemories.erase(foundInPendingMems);

        } else {
           cout<<"NOT FOUND EDGE TO KICK IN PENDING MEMS"<<endl;
        }

    }

    assert(procToChange->availableBuffer >= 0);
}

vector<pair<vertex_t *, double>> calculateBottomLevels(graph_t *graph, int bottomLevelVariant) {
    vector<pair<vertex_t *, double > > ranks;
    switch (bottomLevelVariant) {
        case 1: {
            vertex_t *vertex = graph->first_vertex;
            while (vertex != NULL) {
                double rank = calculateSimpleBottomUpRank(vertex);
                ranks.push_back({vertex, rank});
                vertex = vertex->next;
            }
            break;
        }
        case 2: {
            vertex_t *vertex = graph->first_vertex;
            while (vertex != NULL) {
                double rank = calculateBLCBottomUpRank(vertex);
                ranks.push_back({vertex, rank});
                vertex = vertex->next;
            }
            break;
        }
        case 3:
            ranks = calculateMMBottomUpRank(graph);
            break;
    }
    return ranks;
}


vector<edge_t*>
tentativeAssignmentDespiteMemory(vertex_t *v, Processor *pj, Cluster *cluster, double &finishTime, double &startTime,
                                 bool &isValid, double &peakMem, bool evictBiggestFirst) {

    vector<edge_t* > edgesToKick;

    //step 1
    for (int j = 0; j < v->in_degree; j++) {
        edge *incomingEdge = v->in_edges[j];
        vertex_t *predecessor = incomingEdge->tail;
        if (predecessor->assignedProcessor->id == pj->id) {
            if (std::find_if(pj->pendingMemories.begin(), pj->pendingMemories.end(),
                             [incomingEdge](edge_t *edge) {
                                 return incomingEdge->head->name == edge->head->name &&
                                        incomingEdge->tail->name == edge->tail->name;
                             })
                == pj->pendingMemories.end()) {
               cout<<"Failed because predecessors memory has been evicted on "<< predecessor->assignedProcessor->id<<endl;
               isValid = false;
            }
        }
    }
    //step 2

   // printDebug(" proc "+ to_string(pj->id)+"avail mem "+ to_string(pj->availableMemory));
    double Res = howMuchMemoryIsStillAvailableOnProcIfTaskScheduledThere(v, pj);
    peakMem = (Res<0)? 1:(pj->getMemorySize()-Res)/pj->getMemorySize();

    if(Res <0){
        //evict
      //  printDebug(" need to evict extra "+ to_string(Res));
        double currentlyAvailableBuffer;
        double stillTooMuch;
        stillTooMuch = Res;
        currentlyAvailableBuffer = pj->availableBuffer;
        std::vector<edge_t*> penMemsAsVector;//(pj->pendingMemories.size());
        penMemsAsVector.reserve(pj->pendingMemories.size());
        for (edge_t * e: pj->pendingMemories){
            penMemsAsVector.emplace_back(e);
        }
        std::sort(penMemsAsVector.begin(), penMemsAsVector.end(),[](edge_t* a, edge_t*b) {
                if(a->weight==b->weight){
                    if(a->head->id==b->head->id)
                        return a->tail->id< b->tail->id;
                    else return a->head->id>b->head->id;
                }
                else
                return a->weight<b->weight;
        });
        edgesToKick= evict(penMemsAsVector, currentlyAvailableBuffer, stillTooMuch,
                                                evictBiggestFirst);

       // printInlineDebug(to_string(stillTooMuch));

        if (stillTooMuch < 0 || currentlyAvailableBuffer < 0) {
            // could not evict enough
            printInlineDebug("could not evict enough ");
            if(stillTooMuch<0)printDebug("due to mem ");
            if(currentlyAvailableBuffer<0) printDebug("due to buffer");
            isValid = false;
        }
        else{
           //printInlineDebug("should be successful");
        }
    }
    else{
       // printInlineDebug("should be successful");
    }

    //step 3: tentatively assign

    finishTime = getFinishTimeWithPredecessorsAndBuffers(v, pj, cluster, startTime);
   // cout<<" starttime "<<startTime<<endl;
    return edgesToKick;
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
